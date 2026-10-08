#include "weather.h"
#include "settings.h"
#include <winhttp.h>

std::atomic<bool> g_wxReset{ true };
static Wx g_w; static std::mutex mx;
Wx WxGet() { std::lock_guard<std::mutex> lk(mx); return g_w; }

static std::string http(const wchar_t* host, const std::wstring& path) {
    std::string out; HINTERNET s = WinHttpOpen(L"WinIsland/3.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0); if (!s) return out;
    WinHttpSetTimeouts(s, 5000, 5000, 5000, 8000);
    HINTERNET c = WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path.c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE) : 0;
    if (r && WinHttpSendRequest(r, 0, 0, 0, 0, 0, 0) && WinHttpReceiveResponse(r, 0)) { DWORD n; char b[4096]; while (WinHttpReadData(r, b, sizeof b, &n) && n) out.append(b, n); }
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); WinHttpCloseHandle(s); return out;
}
static std::wstring enc(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, 0, 0, 0, 0); std::string u(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &u[0], n, 0, 0); u.resize(n - 1);
    std::wstring o; for (unsigned char c : u) { if (isalnum(c)) o += (wchar_t)c; else { wchar_t b[4]; swprintf(b, 4, L"%%%02X", c); o += b; } }
    return o;
}
static bool getNum(const std::string& s, const std::string& k, size_t from, double& out) {
    size_t p = s.find("\"" + k + "\":", from); if (p == std::string::npos) return false;
    char* e; out = strtod(s.c_str() + p + k.size() + 3, &e); return e != s.c_str() + p + k.size() + 3;
}
static void getArr(const std::string& s, const std::string& k, size_t from, std::vector<double>& v) {
    size_t p = s.find("\"" + k + "\":[", from); if (p == std::string::npos) return;
    const char* c = s.c_str() + p + k.size() + 4;
    while (*c && *c != ']') { char* e; double d = strtod(c, &e); if (e == c) { while (*c && *c != ',' && *c != ']') c++; v.push_back(0); } else { v.push_back(d); c = e; } if (*c == ',') c++; }
}
static bool isRain(int c) { return (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95; }
const wchar_t* WxSym(int c) {
    if (c == 0) return L"\u2600"; if (c <= 3) return L"\u2601"; if (c == 45 || c == 48) return L"\u2248";
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return L"\u2744"; if (c >= 95) return L"\u26C8"; return L"\u2602";
}

void WeatherThread() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    double lat = 0, lon = 0; std::wstring done; ULONGLONG next = 0; int lastAl = -1000;
    while (g_run) {
        ULONGLONG now = GetTickCount64(); bool force = g_wxReset.exchange(false);
        if (s_wx && (force || now >= next)) {
            std::wstring city = GetCity();
            if (city != done) {
                std::string j = http(L"geocoding-api.open-meteo.com", L"/v1/search?count=1&language=ru&format=json&name=" + enc(city));
                size_t r = j.find("\"results\""); double a, b;
                if (r != std::string::npos && getNum(j, "latitude", r, a) && getNum(j, "longitude", r, b)) { lat = a; lon = b; done = city; }
            }
            next = now + 60000;     // при неудаче повторим через минуту
            if (done == city) {
                wchar_t pth[300];
                swprintf(pth, 300, L"/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&hourly=precipitation_probability,weather_code&forecast_days=2&timezone=auto", lat, lon);
                std::string j = http(L"api.open-meteo.com", pth);
                size_t cp = j.find("\"current\":{"), hp = j.find("\"hourly\":{"); double t, c;
                if (cp != std::string::npos && hp != std::string::npos && getNum(j, "temperature_2m", cp, t) && getNum(j, "weather_code", cp, c)) {
                    size_t tp = j.find("\"time\":\"", cp); int hour = tp != std::string::npos ? atoi(j.c_str() + tp + 8 + 11) : 0;
                    std::vector<double> pp, wc; getArr(j, "precipitation_probability", hp, pp); getArr(j, "weather_code", hp, wc);
                    int rk = 0;
                    if (!isRain((int)c)) for (int k = 1; k <= 2 && !rk; k++) { size_t i = hour + k; if (i < pp.size() && i < wc.size() && (pp[i] >= 50 || isRain((int)wc[i]))) rk = k; }
                    std::lock_guard<std::mutex> lk(mx);
                    g_w.ok = true; g_w.temp = (int)lround(t); g_w.code = (int)c;
                    if (rk && hour + rk != lastAl) {
                        lastAl = hour + rk; wchar_t b[96];
                        swprintf(b, 96, rk == 1 ? L"Через час дождь · с %02d:00" : L"Через 2 ч дождь · с %02d:00", (hour + rk) % 24);
                        g_w.text = b; g_w.seq++;
                    }
                    next = now + 15 * 60000;
                }
            }
        }
        for (int i = 0; i < 5 && g_run && !g_wxReset; i++) Sleep(100);
    }
}
