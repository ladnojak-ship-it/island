#include "island.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "sysinfo.h"
#include "media.h"
#include "weather.h"
#include "anim.h"

static HWND hw; static View V;
static Spring spW, spH, hudSp;
static bool expanded = false, drag = false, menuOpen = false;
static int hud = 0; static ULONGLONG hudEnd = 0, leaveT = 0; static std::wstring hudText;
static float fade = 0, volV = 0; static bool volM = false;
static Bitmap* art = nullptr; static std::wstring lastTitle, lastArtist;
static int lastSeq = 0;
static const wchar_t* LC[4] = { L"explorer.exe", L"wt.exe", L"calc.exe", L"ms-screenclip:" };

void IslandInit(HWND h) { hw = h; spW.snap(150); spH.snap(36); hudSp.snap(1); volV = VolGet(); volM = MuteGet(); }
void IslandMenuOpen(bool o) { menuOpen = o; }

static void CursorXY(float& x, float& y) { POINT p; GetCursorPos(&p); x = (p.x - g_canvasLeft) / g_S; y = p.y / g_S; }
static void SetHud(int kind, ULONGLONG now, int ms) {
    if (hud != kind) { hudSp.x = 0; hudSp.v = 0; hudSp.t = 1; hudSp.k = 260; hudSp.c = 13; }   // иконка «выпрыгивает»
    hud = kind; hudEnd = now + ms;
}
void IslandNotify(const std::wstring& t) { hudText = t; SetHud(4, GetTickCount64(), 3500); }
static void SetVolume(float v) { v = cl(v, 0, 1); VolSet(v); volV = v; volM = false; }

// ---------- ввод ----------
void IslandMouseDown() {
    if (!expanded || spW.x < 400) return;
    float x, y; CursorXY(x, y); Lay L = LayoutFor(spW.x);
    auto d = [&](PointF p, float r) { return std::hypot(x - p.X, y - p.Y) < r; };
    if (d(L.prev, 25)) { MediaSend(2); V.press[0] = 1; }
    else if (d(L.next, 25)) { MediaSend(3); V.press[2] = 1; }
    else if (d(L.play, 27)) { MediaSend(1); V.press[1] = 1; }
    else if (x >= L.prog.X - 6 && x <= L.prog.X + L.prog.Width + 6 && fabsf(y - (L.prog.Y + 2)) < 10) {
        Media m = MediaGet(); if (m.dur > 0) MediaSend(4, cl((x - L.prog.X) / L.prog.Width, 0, 1) * m.dur);
    }
    else if (d(L.mute, 16)) { volM = !volM; MuteSet(volM); }
    else if (fabsf(y - (L.vb.Y + 2.5f)) < 12 && x >= L.vb.X - 8 && x <= L.vb.X + L.vb.Width + 8) { drag = true; SetCapture(hw); SetVolume((x - L.vb.X) / L.vb.Width); }
    else if (s_launch) for (int i = 0; i < 5; i++) if (d(L.launch[i], 20)) {
        V.press[3 + i] = 1;
        if (i == 4) OpenSettings();
        else if ((INT_PTR)ShellExecuteW(0, L"open", LC[i], 0, 0, SW_SHOW) <= 32 && i == 1) ShellExecuteW(0, L"open", L"cmd.exe", 0, 0, SW_SHOW);
        break;
    }
}
void IslandMouseMove() { if (drag) { float x, y; CursorXY(x, y); Lay L = LayoutFor(spW.x); SetVolume((x - L.vb.X) / L.vb.Width); } }
void IslandMouseUp() { if (drag) { drag = false; ReleaseCapture(); } }
void IslandWheel(int delta) { SetVolume(volV + (delta / 120) * 0.02f); }

// ---------- главный цикл логики ----------
int IslandTick() {
    static LARGE_INTEGER fq, last; static bool init = false, firstDraw = true; static ULONGLONG t1 = 0, t2 = 0, lastDraw = 0;
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    if (g_resizeReq.exchange(false)) { RenderResize(); firstDraw = true; }
    if (!init) { QueryPerformanceFrequency(&fq); last = n; init = true; }
    float dt = cl((float)(n.QuadPart - last.QuadPart) / fq.QuadPart, 0.001f, 0.05f); last = n;
    ULONGLONG now = GetTickCount64(); bool am = false, sec = false;
    auto sm = [&](float& c, float t, float rate) { float d = t - c; if (fabsf(d) < .003f) { if (d != 0) { c = t; am = true; } return; } c += d * (1 - expf(-rate * dt)); am = true; };

    if (now - t1 >= 1000) {                     // раз в секунду: система, погода, «поверх всех»
        t1 = now; sec = true; bool had = g_sys.valid, ac0 = g_sys.ac; SysUpdate();
        if (had && g_sys.ac != ac0 && g_sys.bat >= 0 && s_hud) SetHud(2, now, 2500);
        V.wx = WxGet();
        if (V.wx.seq != lastSeq) { lastSeq = V.wx.seq; if (s_wx) { hudText = V.wx.text; SetHud(3, now, 4500); } }
        SetWindowPos(hw, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (now - t2 >= 80) {                       // громкость: если изменили клавишами — показываем всплывашку
        t2 = now; float v = VolGet(); bool mu = MuteGet();
        if (!drag && s_hud && (fabsf(v - volV) > 0.004f || mu != volM)) SetHud(1, now, 1600);
        volV = v; volM = mu;
    }
    if (hud && now > hudEnd) hud = 0;
    bool thm = ThemeStep(dt);

    // наведение: расширяем при входе, сворачиваем через 250 мс после выхода
    float mx, my; CursorXY(mx, my); float rx = (CW - spW.t) / 2;
    bool in = mx >= rx - 6 && mx <= rx + spW.t + 6 && my >= 0 && my <= 8 + spH.t + 8;
    if (in && !menuOpen) { leaveT = 0; expanded = true; }
    else if (expanded && !drag && !menuOpen) { if (!leaveT) leaveT = now; else if (now - leaveT > 250) { expanded = false; leaveT = 0; } }

    Media m = MediaGet(); bool artChanged = MediaTakeArt(art);
    if (m.title != lastTitle || m.artist != lastArtist) { lastTitle = m.title; lastArtist = m.artist; if (m.has) V.trackAnim = 0; }
    if (V.trackAnim < 1) { V.trackAnim = min(1.f, V.trackAnim + dt / .55f); am = true; }

    // цели размеров + разные «характеры» пружин: раскрытие — с отскоком, сворачивание — плотнее, всплывашка — «поп»
    if (expanded) { spW.t = 430; spH.t = s_launch ? 276.f : 240.f; spW.k = 300; spW.c = 19; }
    else if (hud) { spW.t = hud == 3 ? 340.f : hud == 4 ? 360.f : 310.f; spH.t = 50; spW.k = 380; spW.c = 17; }
    else { spW.t = m.has ? 230.f : 158.f; spH.t = m.has ? 40.f : 38.f; spW.k = 360; spW.c = 30; }
    spH.k = spW.k * .85f; spH.c = spW.c * .95f;           // высота чуть запаздывает — «желейный» эффект
    spW.step(dt); spH.step(dt); hudSp.step(dt);
    bool moving = spW.moving() || spH.moving();
    if (!spW.moving()) spW.snap(spW.t); if (!spH.moving()) spH.snap(spH.t);
    if (!hudSp.moving(.002f)) hudSp.snap(hudSp.t);
    float pf = fade; fade = cl(fade + (expanded ? 1 : -1) * dt * 5.5f, 0, 1); if (pf != fade) am = true;

    // плавные значения: наведение, нажатие, play/pause, эквалайзер, громкость
    float hovT[10] = { 0 };
    if (expanded && fade > .6f) {
        Lay L = LayoutFor(spW.x); auto d = [&](PointF p, float r) { return std::hypot(mx - p.X, my - p.Y) < r; };
        hovT[0] = d(L.prev, 25); hovT[1] = d(L.play, 27); hovT[2] = d(L.next, 25);
        if (s_launch) for (int i = 0; i < 5; i++) hovT[3 + i] = d(L.launch[i], 20);
        hovT[8] = (mx >= L.prog.X - 6 && mx <= L.prog.X + L.prog.Width + 6 && fabsf(my - (L.prog.Y + 2)) < 12) ? 1.f : 0.f;
        hovT[9] = (drag || (fabsf(my - (L.vb.Y + 2.5f)) < 12 && mx >= L.vb.X - 8 && mx <= L.vb.X + L.vb.Width + 8)) ? 1.f : 0.f;
    }
    for (int i = 0; i < 10; i++) sm(V.hov[i], hovT[i], 16);
    for (int i = 0; i < 8; i++) sm(V.press[i], 0, 9);
    sm(V.playAnim, m.playing ? 1.f : 0.f, 14); sm(V.eqAmp, m.playing ? 1.f : 0.f, 6);
    sm(V.volDisp, volM ? 0.f : volV, drag ? 40.f : 16.f);
    V.glow = (!s_lite && m.has && V.eqAmp > .01f) ? V.eqAmp * (.65f + .35f * sinf((float)(now / 1000.0) * 2.2f)) : 0.f;

    // решаем, нужно ли рисовать: во время анимаций — каждый кадр, при «живой» музыке — 30 fps, в покое — раз в секунду
    bool cont = V.eqAmp > .01f;
    bool doDraw = am || moving || thm || artChanged || firstDraw;
    if (!doDraw && cont && now - lastDraw >= (s_lite ? 50u : 33u)) doDraw = true;
    if (!doDraw && sec) doDraw = true;
    if (doDraw) {
        V.pw = spW.x; V.ph = spH.x; V.fade = fade; V.hud = hud; V.hudText = hudText; V.hudPop = hudSp.x; V.mute = volM;
        V.m = m; V.sys = g_sys; V.art = art; V.now = now / 1000.0;
        RenderFrame(V); firstDraw = false; lastDraw = now;
    }
    if (am || moving || thm) return 0;
    return cont ? 12 : 40;
}
