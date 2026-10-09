#include "audio.h"
#include "settings.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>

static std::atomic<float> g_b[NB]; static std::atomic<bool> g_live{ false };
void AudioBands(float o[NB]) { for (int i = 0; i < NB; i++) o[i] = g_b[i].load(); }
bool AudioLive() { return g_live.load(); }

static const int N = 1024, HOP = 512;

static void fft(float* re, float* im, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -6.2831853f / len, wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1, ci = 0;
            for (int j = 0; j < len / 2; j++) {
                int a = i + j, b = a + len / 2;
                float vr = re[b] * cr - im[b] * ci, vi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - vr; im[b] = im[a] - vi; re[a] += vr; im[a] += vi;
                float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

struct Cap {
    IMMDeviceEnumerator* en = nullptr; IMMDevice* dev = nullptr; IAudioClient* ac = nullptr; IAudioCaptureClient* cc = nullptr;
    WAVEFORMATEX* wf = nullptr; bool flt = false; bool ok = false;
    void close() {
        if (ac && ok) ac->Stop();
        if (cc) { cc->Release(); cc = nullptr; } if (ac) { ac->Release(); ac = nullptr; } if (dev) { dev->Release(); dev = nullptr; }
        if (en) { en->Release(); en = nullptr; } if (wf) { CoTaskMemFree(wf); wf = nullptr; } ok = false;
    }
    bool open() {
        close();
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return false;
        if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) return false;
        if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ac))) return false;
        if (FAILED(ac->GetMixFormat(&wf))) return false;
        flt = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)wf)->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT);
        if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 2000000, 0, wf, nullptr))) return false;
        if (FAILED(ac->GetService(__uuidof(IAudioCaptureClient), (void**)&cc))) return false;
        if (FAILED(ac->Start())) return false;
        ok = true; return true;
    }
};

void AudioThread() {
    CoInitializeEx(0, COINIT_MULTITHREADED);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    Cap cap; static float ring[N]; int wpos = 0, since = 0, idle = 0; static float sm[NB];
    static float re[N], im[N], win[N];
    for (int i = 0; i < N; i++) win[i] = .5f - .5f * cosf(6.2831853f * i / (N - 1));
    auto decay = [&](float k) { for (int i = 0; i < NB; i++) { sm[i] *= k; g_b[i] = sm[i]; } };
    auto analyze = [&](float sr) {
        for (int i = 0; i < N; i++) { re[i] = ring[(wpos + i) % N] * win[i]; im[i] = 0; }
        fft(re, im, N);
        float fhi = min(16000.f, sr * .47f), flo = 45.f;
        for (int b = 0; b < NB; b++) {
            float e0 = flo * powf(fhi / flo, (float)b / NB), e1 = flo * powf(fhi / flo, (float)(b + 1) / NB);
            int k0 = max(1, (int)floorf(e0 * N / sr)), k1 = max(k0 + 1, (int)ceilf(e1 * N / sr)); k1 = min(k1, N / 2);
            float mx = 0; for (int k = k0; k < k1; k++) mx = max(mx, 4.f * sqrtf(re[k] * re[k] + im[k] * im[k]) / N);
            float db = 20.f * log10f(mx + 1e-7f) + 1.0f * b;          // небольшой наклон: высокие частоты тише
            float v = cl((db + 58.f) / 46.f, 0, 1); v = v * v * (3 - 2 * v) * .35f + v * .65f;
            if (v > sm[b]) sm[b] += (v - sm[b]) * .65f; else sm[b] += (v - sm[b]) * .12f;
            g_b[b] = sm[b];
        }
    };
    while (g_run) {
        if (!s_viz) { cap.close(); g_live = false; decay(.8f); Sleep(100); continue; }
        if (!cap.ok) { if (!cap.open()) { cap.close(); g_live = false; decay(.8f); for (int i = 0; i < 10 && g_run; i++) Sleep(100); continue; } g_live = true; }
        UINT32 n = 0; if (FAILED(cap.cc->GetNextPacketSize(&n))) { cap.close(); g_live = false; continue; }
        if (n == 0) { if (++idle > 4) decay(.88f); Sleep(8); continue; }
        idle = 0;
        const int ch = max(1, (int)cap.wf->nChannels), bits = cap.wf->wBitsPerSample; const float sr = (float)cap.wf->nSamplesPerSec;
        while (n) {
            BYTE* d = nullptr; UINT32 fr = 0; DWORD fl = 0;
            if (FAILED(cap.cc->GetBuffer(&d, &fr, &fl, nullptr, nullptr))) break;
            for (UINT32 f = 0; f < fr; f++) {
                float s = 0;
                if (!(fl & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    for (int c = 0; c < ch; c++) {
                        if (cap.flt && bits == 32) s += ((float*)d)[f * ch + c];
                        else if (bits == 16) s += ((short*)d)[f * ch + c] / 32768.f;
                        else if (bits == 32) s += ((int*)d)[f * ch + c] / 2147483648.f;
                        else if (bits == 24) { BYTE* q = d + (f * ch + c) * 3; int v = (q[0] << 8) | (q[1] << 16) | (q[2] << 24); s += v / 2147483648.f; }
                    }
                    s /= ch;
                }
                ring[wpos] = s; wpos = (wpos + 1) % N;
                if (++since >= HOP) { since = 0; analyze(sr); }
            }
            cap.cc->ReleaseBuffer(fr);
            if (FAILED(cap.cc->GetNextPacketSize(&n))) { n = 0; }
        }
    }
    cap.close(); CoUninitialize();
}
