#include "sysinfo.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>

Sys g_sys;
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
