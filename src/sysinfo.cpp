#include "sysinfo.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <cmath>

Sys g_sys;
float g_eqBands[EQ_BANDS] = {};
static IAudioEndpointVolume* vol = nullptr;

void SysUpdate() {
    FILETIME i, k, u; GetSystemTimes(&i, &k, &u);
    auto f = [](FILETIME a) { return ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime; };
    static ULONGLONG pI = 0, pT = 0; ULONGLONG I = f(i), T = f(k) + f(u);
    if (T > pT) g_sys.cpu = 100.f * (1.f - (float)(I - pI) / (float)(T - pT));
    pI = I; pT = T;
    MEMORYSTATUSEX ms{ sizeof(ms) }; GlobalMemoryStatusEx(&ms); g_sys.ram = (int)ms.dwMemoryLoad;
    SYSTEM_POWER_STATUS sp;
    if (GetSystemPowerStatus(&sp)) { g_sys.bat = sp.BatteryLifePercent == 255 ? -1 : sp.BatteryLifePercent; g_sys.ac = sp.ACLineStatus == 1; }
    g_sys.valid = true;
}
bool VolInit() {
    IMMDeviceEnumerator* e = nullptr; IMMDevice* d = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, IID_PPV_ARGS(&e))) && SUCCEEDED(e->GetDefaultAudioEndpoint(eRender, eConsole, &d)))
        return SUCCEEDED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, 0, (void**)&vol));
    return false;
}
float VolGet() { float v = 0; if (vol) vol->GetMasterVolumeLevelScalar(&v); return v; }
bool MuteGet() { BOOL m = FALSE; if (vol) vol->GetMute(&m); return m != 0; }
void VolSet(float v) { if (vol) { vol->SetMasterVolumeLevelScalar(cl(v, 0, 1), NULL); vol->SetMute(FALSE, NULL); } }
void MuteSet(bool m) { if (vol) vol->SetMute(m, NULL); }

// ─── WASAPI loopback → 8-полосный EQ (sub, bass, low-mid, mid, high-mid, presence, air, brilliance) ───
static std::atomic<bool> g_eqRun{ false };
static std::thread       g_eqThread;

// Границы полос (Гц): sub 20-60, bass 60-250, low-mid 250-500, mid 500-2k,
//                     high-mid 2k-4k, presence 4k-6k, air 6k-12k, brilliance 12k-20k
static const float BAND_LO[EQ_BANDS] = { 20,  60, 250,  500, 2000, 4000,  6000, 12000 };
static const float BAND_HI[EQ_BANDS] = { 60, 250, 500, 2000, 4000, 6000, 12000, 20000 };

static void EqThread() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator* enm = nullptr;
    IMMDevice*           dev = nullptr;
    IAudioClient*        ac  = nullptr;
    IAudioCaptureClient* cc  = nullptr;

    auto cleanup = [&]() { if (cc) cc->Release(); if (ac) { ac->Stop(); ac->Release(); } if (dev) dev->Release(); if (enm) enm->Release(); };

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enm)))) { CoUninitialize(); return; }
    if (FAILED(enm->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) { cleanup(); CoUninitialize(); return; }

    WAVEFORMATEX* wfx = nullptr;
    if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ac))) { cleanup(); CoUninitialize(); return; }
    ac->GetMixFormat(&wfx);
    // loopback: AUDCLNT_STREAMFLAGS_LOOPBACK
    HRESULT hr = ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 200000, 0, wfx, nullptr);
    if (FAILED(hr)) { CoTaskMemFree(wfx); cleanup(); CoUninitialize(); return; }
    if (FAILED(ac->GetService(IID_PPV_ARGS(&cc)))) { CoTaskMemFree(wfx); cleanup(); CoUninitialize(); return; }
    ac->Start();

    int   SR     = (int)wfx->nSamplesPerSec;
    int   CH     = (int)wfx->nChannels;
    bool  isFloat = (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                    (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    CoTaskMemFree(wfx);

    // простой DFT-блок: накапливаем до ~20мс, считаем RMS по каждой полосе через goertzel-like подход
    // Для скорости: 512 samples sliding buffer, simple magnitude via running sum
    const int BUFN = 512;
    std::vector<float> buf(BUFN, 0.f);
    int bufPos = 0;

    float smoothed[EQ_BANDS] = {};

    while (g_eqRun) {
        UINT32 padding = 0;
        if (FAILED(ac->GetCurrentPadding(&padding))) break;

        BYTE*  data   = nullptr;
        UINT32 frames = 0;
        DWORD  flags  = 0;
        while (SUCCEEDED(cc->GetBuffer(&frames, &data, &flags, nullptr, nullptr)) && frames > 0) {
            for (UINT32 i = 0; i < frames; i++) {
                float s = 0.f;
                if (isFloat) {
                    float* p = (float*)data + i * CH;
                    for (int c = 0; c < CH; c++) s += p[c];
                } else {
                    int16_t* p = (int16_t*)data + i * CH;
                    for (int c = 0; c < CH; c++) s += p[c] / 32768.f;
                }
                buf[bufPos % BUFN] = s / CH;
                bufPos++;
            }
            cc->ReleaseBuffer(frames);
        }

        // compute per-band power via DFT at band-center frequencies
        if (bufPos >= BUFN) {
            float raw[EQ_BANDS] = {};
            for (int b = 0; b < EQ_BANDS; b++) {
                float fc = sqrtf(BAND_LO[b] * BAND_HI[b]);  // geometric mean
                float w  = 2.f * 3.14159265f * fc / SR;
                float re = 0.f, im = 0.f;
                int N = min(BUFN, (int)(SR / BAND_LO[b]));  // adaptive window
                N = min(N, BUFN);
                for (int n = 0; n < N; n++) {
                    float x = buf[(bufPos - N + n + BUFN) % BUFN];
                    re += x * cosf(w * n);
                    im += x * sinf(w * n);
                }
                raw[b] = sqrtf(re * re + im * im) / N;
            }
            // smooth + scale
            for (int b = 0; b < EQ_BANDS; b++) {
                float gain = 2.5f + b * 0.4f;  // boost highs
                float v = cl(raw[b] * gain, 0.f, 1.f);
                float spd = (v > smoothed[b]) ? 0.6f : 0.15f;  // fast attack, slow release
                smoothed[b] += (v - smoothed[b]) * spd;
                g_eqBands[b] = smoothed[b];
            }
        }
        Sleep(16);  // ~60fps
    }
    cleanup();
    CoUninitialize();
}

void EqCaptureStart() {
    if (g_eqRun) return;
    g_eqRun = true;
    g_eqThread = std::thread(EqThread);
}
void EqCaptureStop() {
    g_eqRun = false;
    if (g_eqThread.joinable()) g_eqThread.join();
}
