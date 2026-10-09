#include "theme.h"
#include "settings.h"

float thTgt[6][3] = { {0,0,0},{255,255,255},{255,255,255},{255,255,255},{0,0,0},{44,44,46} };
static float thCur[6][3] = { {0,0,0},{255,255,255},{255,255,255},{255,255,255},{0,0,0},{44,44,46} };
std::atomic<bool> g_themeDirty{ false };
std::atomic<int> g_wpVer{ 0 };
static std::atomic<float> g_wpHue{ 265 };

// ---- тональные палитры в CIELAB (L* = tone, как в HCT) ----
struct V3 { float r, g, b; };
static float lin2s(float c) { return c <= 0.0031308f ? 12.92f * c : 1.055f * powf(c, 1 / 2.4f) - 0.055f; }
static bool lab2rgb(float L, float a, float b, V3& o) {
    float fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
    auto f = [](float t) { float t3 = t * t * t; return t3 > 0.008856f ? t3 : (t - 16.f / 116) / 7.787f; };
    float X = .95047f * f(fx), Y = f(fy), Z = 1.08883f * f(fz);
    float r = 3.2406f * X - 1.5372f * Y - .4986f * Z, g = -.9689f * X + 1.8758f * Y + .0415f * Z, bl = .0557f * X - .2040f * Y + 1.0570f * Z;
    if (r < -.001f || r > 1.001f || g < -.001f || g > 1.001f || bl < -.001f || bl > 1.001f) return false;
    o = { cl(lin2s(cl(r, 0, 1)), 0, 1) * 255, cl(lin2s(cl(g, 0, 1)), 0, 1) * 255, cl(lin2s(cl(bl, 0, 1)), 0, 1) * 255 };
    return true;
}
static V3 tone(float h, float c, float t) {
    if (t <= 0) return { 0,0,0 }; if (t >= 100) return { 255,255,255 };
    float hr = h * 3.14159265f / 180;
    for (;; c -= 1) { if (c < 0) c = 0; V3 o; if (lab2rgb(t, c * cosf(hr), c * sinf(hr), o)) return o; if (c == 0) return { 128,128,128 }; }
}
static float hueOf(float r, float g, float b) {
    auto s2l = [](float c) { c /= 255; return c <= 0.04045f ? c / 12.92f : powf((c + .055f) / 1.055f, 2.4f); };
    float R = s2l(r), G = s2l(g), B = s2l(b);
    float X = (.4124f * R + .3576f * G + .1805f * B) / .95047f, Y = .2126f * R + .7152f * G + .0722f * B, Z = (.0193f * R + .1192f * G + .9505f * B) / 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? cbrtf(t) : 7.787f * t + 16.f / 116; };
    float a = 500 * (f(X) - f(Y)), bb = 200 * (f(Y) - f(Z)), h = atan2f(bb, a) * 180 / 3.14159265f;
    return h < 0 ? h + 360 : h;
}
static void makeMY(float h, bool dark, float o[6][3]) {
    auto set = [&](int i, V3 v) { o[i][0] = v.r; o[i][1] = v.g; o[i][2] = v.b; };
    if (dark) { set(0, tone(h, 10, 9)); set(1, tone(h, 6, 90)); set(2, tone(h, 36, 80)); set(3, tone(h, 36, 80)); set(4, tone(h, 36, 20)); set(5, tone(h, 16, 30)); }
    else { set(0, tone(h, 8, 95)); set(1, tone(h, 6, 10)); set(2, tone(h, 36, 40)); set(3, tone(h, 36, 40)); set(4, { 255,255,255 }); set(5, tone(h, 16, 90)); }
}
void ThemePreview(int theme, float o[6][3]) {
    if (theme == 0) { static const float A[6][3] = { {0,0,0},{255,255,255},{255,255,255},{255,255,255},{0,0,0},{44,44,46} }; memcpy(o, A, sizeof A); }
    else makeMY(s_accent >= 0 ? (float)s_accent : g_wpHue.load(), s_dark != 0, o);
}
Color ThemeSeed(float hue) { V3 v = tone(hue, 40, 62); return Color(255, (BYTE)v.r, (BYTE)v.g, (BYTE)v.b); }
float ThemeWpHue() { return g_wpHue.load(); }
static void ApplyTheme() {
    if (s_theme == 0) { static const float A[6][3] = { {0,0,0},{255,255,255},{255,255,255},{255,255,255},{0,0,0},{44,44,46} }; memcpy(thTgt, A, sizeof A); }
    else makeMY(s_accent >= 0 ? (float)s_accent : g_wpHue.load(), s_dark != 0, thTgt);
}
void ThemeInit() { ApplyTheme(); memcpy(thCur, thTgt, sizeof thCur); }
bool ThemeStep(float dt) {
    static int lastWp = -1;
    if (g_themeDirty.exchange(false) || g_wpVer.load() != lastWp) { lastWp = g_wpVer.load(); ApplyTheme(); }
    bool mv = false; float k = 1 - expf(-dt * 7);
    for (int i = 0; i < 6; i++) for (int j = 0; j < 3; j++) {
        float d = thTgt[i][j] - thCur[i][j];
        if (fabsf(d) < .4f) thCur[i][j] = thTgt[i][j]; else { thCur[i][j] += d * k; mv = true; }
    }
    return mv;
}
Color TC(int i, float a) { return Color((BYTE)(cl(a, 0, 1) * 255), (BYTE)thCur[i][0], (BYTE)thCur[i][1], (BYTE)thCur[i][2]); }
Color Al(Color c, float a) { return Color((BYTE)(cl(a, 0, 1) * 255), c.GetR(), c.GetG(), c.GetB()); }

// ---- обои -> доминирующий оттенок (фоновый поток, следит за сменой файла/слайдшоу) ----
static bool wpPath(std::wstring& p) {
    wchar_t b[MAX_PATH] = { 0 }; SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, b, 0); p = b;
    if (p.empty() || GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wchar_t a[MAX_PATH]; if (!GetEnvironmentVariableW(L"APPDATA", a, MAX_PATH)) return false;
        p = std::wstring(a) + L"\\Microsoft\\Windows\\Themes\\TranscodedWallpaper";
        if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    }
    return true;
}
static bool wpCalc(const std::wstring& p, float& hue) {
    Bitmap src(p.c_str()); if (src.GetLastStatus() != Ok) return false;
    Bitmap sm(48, 48, PixelFormat32bppARGB);
    { Graphics gg(&sm); gg.SetInterpolationMode(InterpolationModeBilinear); gg.DrawImage(&src, 0, 0, 48, 48); }
    BitmapData bd; Rect rc(0, 0, 48, 48);
    if (sm.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) return false;
    double w[24] = { 0 }, sr[24] = { 0 }, sg[24] = { 0 }, sb[24] = { 0 }, tot = 0;
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) {
        BYTE* q = (BYTE*)bd.Scan0 + y * bd.Stride + x * 4; float b = q[0], g = q[1], r = q[2];
        float mx = max(r, max(g, b)), mn = min(r, min(g, b)), v = mx / 255, s = mx > 0 ? (mx - mn) / mx : 0;
        if (v < .2f || s < .25f) continue;
        float d = mx - mn, hh = mx == r ? fmodf((g - b) / d, 6) : mx == g ? (b - r) / d + 2 : (r - g) / d + 4; hh *= 60; if (hh < 0) hh += 360;
        int bin = ((int)(hh / 15)) % 24; double wt = s * v;
        w[bin] += wt; sr[bin] += r * wt; sg[bin] += g * wt; sb[bin] += b * wt; tot += wt;
    }
    sm.UnlockBits(&bd);
    if (tot < 10) return false;
    int best = 0; double bs = -1;
    for (int i = 0; i < 24; i++) { double sc = w[i] + .5 * (w[(i + 23) % 24] + w[(i + 1) % 24]); if (sc > bs) { bs = sc; best = i; } }
    if (w[best] <= 0) return false;
    hue = hueOf((float)(sr[best] / w[best]), (float)(sg[best] / w[best]), (float)(sb[best] / w[best]));
    return true;
}
void WallpaperThread() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    std::wstring last; FILETIME lt{};
    while (g_run) {
        std::wstring p;
        if (wpPath(p)) {                      // считаем всегда: палитра нужна и для превью в настройках
            WIN32_FILE_ATTRIBUTE_DATA d; FILETIME t{}; if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) t = d.ftLastWriteTime;
            if (p != last || CompareFileTime(&t, &lt) != 0) { last = p; lt = t; float h; if (wpCalc(p, h)) { g_wpHue = h; g_wpVer++; } }
        }
        for (int i = 0; i < 20 && g_run; i++) Sleep(100);
    }
}
