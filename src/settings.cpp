#include "settings.h"
#include "theme.h"
#include "weather.h"
#include "render.h"
#include "fonts.h"
#include "icons.h"
#include "gfx.h"
#include <dwmapi.h>

std::atomic<int> s_theme{ 0 }, s_dark{ 1 }, s_wx{ 1 }, s_launch{ 1 }, s_art{ 1 }, s_hud{ 1 }, s_lite{ 0 }, s_scale{ 115 },
    s_round{ 0 }, s_auto{ 1 }, s_hide{ 4 }, s_top{ 0 }, s_accent{ -1 }, s_sys{ 1 }, s_viz{ 1 }, s_shot{ 1 };
static std::wstring s_city = L"Москва"; static std::mutex cm;
static const wchar_t* RK = L"Software\\WinIsland";

static int rdI(const wchar_t* n, int d) { DWORD v = 0, sz = 4; return RegGetValueW(HKEY_CURRENT_USER, RK, n, RRF_RT_REG_DWORD, 0, &v, &sz) == ERROR_SUCCESS ? (int)v : d; }
static void wrI(const wchar_t* n, int v) { DWORD d = v; RegSetKeyValueW(HKEY_CURRENT_USER, RK, n, REG_DWORD, &d, 4); }
std::wstring GetCity() { std::lock_guard<std::mutex> lk(cm); return s_city; }
void LoadSettings() {
    s_theme = rdI(L"theme", 0); s_dark = rdI(L"dark", 1); s_wx = rdI(L"wx", 1);
    s_launch = rdI(L"launch", 1); s_art = rdI(L"art", 1); s_hud = rdI(L"hud", 1); s_lite = rdI(L"lite", 0);
    s_scale = rdI(L"scale", 115); if (s_scale < 80 || s_scale > 200) s_scale = 115;
    s_round = rdI(L"round", 0); s_auto = rdI(L"auto", 1); s_hide = rdI(L"hide", 4); s_top = rdI(L"top", 0); s_accent = rdI(L"accent", -1);
    s_sys = rdI(L"sys", 1); s_viz = rdI(L"viz", 1); s_shot = rdI(L"shot", 1);
    wchar_t b[64]; DWORD sz = sizeof b;
    if (RegGetValueW(HKEY_CURRENT_USER, RK, L"city", RRF_RT_REG_SZ, 0, b, &sz) == ERROR_SUCCESS) { std::lock_guard<std::mutex> lk(cm); s_city = b; }
}
void SaveSettings() {
    wrI(L"theme", s_theme); wrI(L"dark", s_dark); wrI(L"wx", s_wx); wrI(L"launch", s_launch); wrI(L"art", s_art); wrI(L"hud", s_hud); wrI(L"lite", s_lite); wrI(L"scale", s_scale);
    wrI(L"round", s_round); wrI(L"auto", s_auto); wrI(L"hide", s_hide); wrI(L"top", s_top); wrI(L"accent", s_accent); wrI(L"sys", s_sys); wrI(L"viz", s_viz); wrI(L"shot", s_shot);
    std::lock_guard<std::mutex> lk(cm);
    RegSetKeyValueW(HKEY_CURRENT_USER, RK, L"city", REG_SZ, s_city.c_str(), (DWORD)((s_city.size() + 1) * 2));
}

// =====================================================================================
//  Окно настроек: рисуется целиком через GDI+ в стиле выбранной темы (iOS / Material You), с прокруткой
// =====================================================================================
static const float W = 380, H = 1126;                        // логические размеры содержимого
static const int ID_CITY = 1001;
enum { T_DARK, T_AUTO, T_SYS, T_VIZ, T_SHOT, T_LITE, T_WX, T_LAUNCH, T_ART, T_HUD, T_N };
enum { S_SIZE, S_ROUND, S_TOP, S_DELAY, S_N };
enum { H_NONE = -1, H_CLOSE = 0, H_CARD = 1, H_TG = 10, H_ACC = 30, H_SEG = 40 };
static const int SCALES[4] = { 100, 115, 130, 150 }, TOPS[4] = { 0, 8, 16, 24 }, DELAYS[4] = { 2, 4, 8, 15 }, ACCS[6] = { 35, 65, 135, 195, 290, 350 };
static const int SEGN[S_N] = { 4, 3, 4, 4 };
static const wchar_t* TLB[T_N] = { L"Тёмная схема", L"Автоскрытие", L"CPU, RAM и батарея", L"Визуализатор звука", L"Анимация скриншота", L"Лёгкий режим (без теней)",
    L"Погода и дождь", L"Быстрые приложения", L"Обложка альбома", L"Всплывашки громкости и батареи" };
static const wchar_t* RLB[3] = { L"Круглый", L"Мягкий", L"Строгий" };

static HWND hs = nullptr, hEdit = nullptr; static float SS = 1, WH = 700;       // SS — масштаб окна, WH — логическая высота окна
static int hot = H_NONE; static bool cityFocus = false, tracking = false, editVis = true;
static float tg[T_N] = { 0 }, segA[S_N] = { 0 }, scr = 0, scrT = 0; static DWORD lastSig = 0;
static Font *sT = nullptr, *sB = nullptr, *sS = nullptr, *sH = nullptr; static int sFontTheme = -1;
static HFONT hEditFont = nullptr; static HBRUSH editBr = nullptr; static COLORREF editCol = 0;

static int P(float v) { return (int)(v * SS + .5f); }
static std::atomic<int>* Flag(int t) {
    switch (t) { case T_DARK: return &s_dark; case T_AUTO: return &s_auto; case T_SYS: return &s_sys; case T_VIZ: return &s_viz; case T_SHOT: return &s_shot;
    case T_LITE: return &s_lite; case T_WX: return &s_wx; case T_LAUNCH: return &s_launch; case T_ART: return &s_art; default: return &s_hud; }
}
static int Nearest(const int* a, int v) { int b = 0, bd = 100000; for (int i = 0; i < 4; i++) { int d = abs(a[i] - v); if (d < bd) { bd = d; b = i; } } return b; }
static int SegGet(int s) { switch (s) { case S_SIZE: return Nearest(SCALES, s_scale); case S_ROUND: return min(2, max(0, s_round.load())); case S_TOP: return Nearest(TOPS, s_top); default: return Nearest(DELAYS, s_hide); } }
static void SegSet(int s, int i) { switch (s) { case S_SIZE: s_scale = SCALES[i]; g_resizeReq = true; break; case S_ROUND: s_round = i; break; case S_TOP: s_top = TOPS[i]; break; default: s_hide = DELAYS[i]; } }
static int AccGet() { if (s_accent < 0) return 0; for (int i = 0; i < 6; i++) if (ACCS[i] == s_accent) return i + 1; return 0; }

// ---- геометрия содержимого (логические px) ----
static RectF rClose() { return RectF(W - 52, 14, 32, 32); }
static RectF rCard(int i) { return RectF(20 + i * 176.f, 84, 164, 80); }
static RectF rG1() { return RectF(20, 174, 340, 82); }
static RectF rAccG() { return RectF(20, 290, 340, 56); }
static RectF rAcc(int i) { return RectF(49 + i * 42.f, 303, 30, 30); }
static RectF rG3() { return RectF(20, 380, 340, 210); }
static RectF rG2() { return RectF(20, 624, 340, 210); }
static RectF rTg(int t) { return t == T_DARK ? RectF(20, 174, 340, 42) : t <= T_LITE ? RectF(20, 380 + (t - T_AUTO) * 42.f, 340, 42) : RectF(20, 624 + (t - T_WX) * 42.f, 340, 42); }
static RectF rSegRow(int s) { return RectF(20, 868 + s * 68.f, 340, 34); }
static RectF rSeg(int s, int i) { float w = 340.f / SEGN[s]; return RectF(20 + i * w, 868 + s * 68.f, w, 34); }
static RectF rField() { return RectF(192, 624 + 4 * 42.f + 5, 152, 32); }
static bool In(RectF r, float x, float y) { return x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height; }
static float MaxScroll() { return max(0.f, H - WH); }

// ---- палитра окна ----
struct Pal { Color bg, card, text, sub, acc, onAcc, line, field, chip; bool MY; };
static Pal GetPal() {
    Pal p; p.MY = s_theme != 0;
    if (!p.MY) {
        p.bg = Color(255, 0, 0, 0); p.card = Color(255, 28, 28, 30); p.text = Color(255, 255, 255, 255); p.sub = Color(255, 152, 152, 158);
        p.acc = Color(255, 10, 132, 255); p.onAcc = Color(255, 255, 255, 255); p.line = Color(255, 56, 56, 58); p.chip = Color(255, 72, 72, 74);
    } else {
        p.bg = TC(0, 1); p.text = TC(1, 1); p.acc = TC(2, 1); p.onAcc = TC(4, 1); p.chip = TC(5, 1);
        p.card = Mix(p.bg, p.text, .07f); p.sub = Mix(p.bg, p.text, .68f); p.line = Mix(p.bg, p.text, .14f);
    }
    p.field = Mix(p.card, p.text, .10f);
    return p;
}
static COLORREF CR(Color c) { return RGB(c.GetR(), c.GetG(), c.GetB()); }

static void BuildFonts() {
    int th = s_theme ? 1 : 0; if (th == sFontTheme && sT) return;
    delete sT; delete sB; delete sS; delete sH;
    sT = MkFont(th, 21.f, true); sB = MkFont(th, 15.f, false); sS = MkFont(th, 12.5f, false); sH = MkFont(th, 13.f, true);
    sFontTheme = th;
    if (hEdit) {
        HFONT nf = CreateFontW(-P(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, FontFamilyName(th).c_str());
        SendMessage(hEdit, WM_SETFONT, (WPARAM)nf, TRUE); if (hEditFont) DeleteObject(hEditFont); hEditFont = nf;
    }
}
static void Tx(Graphics& g, const wchar_t* s, Font* f, RectF r, Color c, StringAlignment al = StringAlignmentNear) {
    StringFormat sf(StringFormatFlagsNoWrap); sf.SetAlignment(al); sf.SetLineAlignment(StringAlignmentCenter); sf.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush b(c); g.DrawString(s, -1, f, r, &sf, &b);
}
static void FillRR(Graphics& g, RectF r, float rad, Color c) { GraphicsPath p; RR(p, r, rad); SolidBrush b(c); g.FillPath(&b, &p); }
static void StrokeRR(Graphics& g, RectF r, float rad, Color c, float w) { GraphicsPath p; RR(p, r, rad); Pen pen(c, w); g.DrawPath(&pen, &p); }

static void Toggle(Graphics& g, const Pal& Pl, float rx, float cy, float t, float dis) {
    auto D = [&](Color c) { return dis > 0 ? Mix(c, Pl.card, dis) : c; };
    const float w = 48, h = 28; RectF tr(rx - w, cy - h / 2, w, h);
    if (!Pl.MY) {
        FillRR(g, tr, h / 2, D(Mix(Color(255, 70, 70, 74), Color(255, 48, 209, 88), t)));
        float kx = tr.X + 2 + t * 20;
        SolidBrush sh(Color(70, 0, 0, 0)); g.FillEllipse(&sh, kx, cy - 12 + 1.5f, 24.f, 24.f);
        SolidBrush kb(D(Color(255, 255, 255, 255))); g.FillEllipse(&kb, kx, cy - 12, 24.f, 24.f);
    } else {
        Color off = Mix(Pl.card, Pl.text, .12f);
        FillRR(g, tr, h / 2, D(Mix(off, Pl.acc, t)));
        if (t < .99f) StrokeRR(g, RectF(tr.X + 1, tr.Y + 1, w - 2, h - 2), h / 2 - 1, D(Mix(Pl.sub, Pl.acc, t)), 2.f);
        float d = 14 + 8 * t, kx = tr.X + 14 + t * (w - 28);
        Color kc = D(Mix(Pl.sub, Pl.onAcc, t));
        SolidBrush kb(kc); g.FillEllipse(&kb, kx - d / 2, cy - d / 2, d, d);
        if (t > .35f) DrawIcon(g, IC_CHECK, kx, cy, 13, D(Mix(kc, Pl.acc, (t - .35f) / .65f)), 0);
    }
}
static void Header(Graphics& g, const Pal& Pl, float y, const wchar_t* sentence, const wchar_t* upper) {
    if (Pl.MY) Tx(g, sentence, sH, RectF(28, y, 300, 22), Pl.acc);
    else Tx(g, upper, sS, RectF(32, y, 300, 22), Pl.sub);
}
static void RowHover(Graphics& g, const Pal& Pl, RectF grp, float R, RectF row) {
    GraphicsState st = g.Save(); GraphicsPath gp; RR(gp, grp, R); g.SetClip(&gp);
    SolidBrush b(Mix(Pl.card, Pl.text, .05f)); g.FillRectangle(&b, row); g.Restore(st);
}
static void ToggleRow(Graphics& g, const Pal& Pl, int t, float dis) {
    RectF r = rTg(t); Tx(g, TLB[t], sB, RectF(36, r.Y, 250, r.Height), Mix(Pl.text, Pl.card, dis)); Toggle(g, Pl, 344, r.Y + r.Height / 2, tg[t], dis);
}
static void SegRow(Graphics& g, const Pal& Pl, int s, float dis) {
    const bool MY = Pl.MY; RectF row = rSegRow(s); float n = (float)SEGN[s], w = 340.f / n;
    FillRR(g, row, MY ? 17.f : 9.f, Pl.card);
    FillRR(g, RectF(20 + segA[s] * w + 3, row.Y + 3, w - 6, 28), MY ? 14.f : 7.f, Mix(Pl.chip, Pl.card, dis));
    int cur = SegGet(s);
    for (int i = 0; i < SEGN[s]; i++) {
        wchar_t b[24];
        switch (s) { case S_SIZE: swprintf(b, 24, L"%d%%", SCALES[i]); break; case S_ROUND: swprintf(b, 24, L"%ls", RLB[i]); break;
            case S_TOP: swprintf(b, 24, L"%d", TOPS[i]); break; default: swprintf(b, 24, L"%d с", DELAYS[i]); }
        bool sel = cur == i, hv = hot == H_SEG + s * 8 + i;
        Color c = sel || hv ? Pl.text : Pl.sub; if (dis > 0) c = Mix(c, Pl.card, dis);
        Tx(g, b, sB, rSeg(s, i), c, StringAlignmentCenter);
    }
}

static void Paint(HDC dc) {
    RECT rc; GetClientRect(hs, &rc); int Wp = rc.right, Hp = rc.bottom; if (Wp <= 0 || Hp <= 0) return;
    BuildFonts(); Pal Pl = GetPal(); const bool MY = Pl.MY; const float R = MY ? 20.f : 12.f; const int IS = MY ? 1 : 2;
    Bitmap bmp(Wp, Hp, PixelFormat32bppRGB); Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeHighQuality); g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(Pl.bg); g.ScaleTransform(SS, SS);

    // ===== содержимое (прокручивается) =====
    g.TranslateTransform(0, -scr); g.SetClip(RectF(0, scr + 60, W, WH - 60), CombineModeReplace);
    // ---- тема ----
    Header(g, Pl, 60, L"Тема", L"ТЕМА");
    float pa[6][3]; ThemePreview(1, pa);
    auto pc = [&](int i) { return Color(255, (BYTE)pa[i][0], (BYTE)pa[i][1], (BYTE)pa[i][2]); };
    for (int i = 0; i < 2; i++) {
        RectF r = rCard(i); bool sel = (s_theme != 0) == (i == 1);
        FillRR(g, r, R, hot == H_CARD + i && !sel ? Mix(Pl.card, Pl.text, .05f) : Pl.card);
        if (sel) StrokeRR(g, RectF(r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2), R - 1, Pl.acc, 2.f);
        float x0 = r.X + 16, y0 = r.Y + 12, w = r.Width - 32, h = 30;
        Color pb = i == 0 ? Color(255, 0, 0, 0) : pc(0);
        FillRR(g, RectF(x0, y0, w, h), h / 2, pb); StrokeRR(g, RectF(x0, y0, w, h), h / 2, Mix(Pl.card, Pl.text, .22f), 1.f);
        FillRR(g, RectF(x0 + 6, y0 + 5, 20, 20), i == 0 ? 6.f : 8.f, i == 0 ? Color(255, 255, 95, 120) : pc(2));
        FillRR(g, RectF(x0 + 32, y0 + 8, 46, 5), 2.5f, i == 0 ? Color(255, 235, 235, 235) : pc(1));
        FillRR(g, RectF(x0 + 32, y0 + 17, 30, 4), 2.f, i == 0 ? Color(255, 110, 110, 114) : Mix(pc(0), pc(1), .5f));
        { SolidBrush b(i == 0 ? Color(255, 255, 255, 255) : pc(3)); g.FillEllipse(&b, x0 + w - 27, y0 + 6, 18.f, 18.f); }
        DrawIcon(g, IC_PLAY, x0 + w - 18, y0 + 15, 11, i == 0 ? Color(255, 0, 0, 0) : pc(4), IS);
        Tx(g, i ? L"Material You" : L"Apple", sB, RectF(r.X + 16, r.Y + 46, r.Width - 56, 26), Pl.text);
        if (sel) { SolidBrush b(Pl.acc); g.FillEllipse(&b, r.X + r.Width - 34, r.Y + 49, 20.f, 20.f); DrawIcon(g, IC_CHECK, r.X + r.Width - 24, r.Y + 59, 14, Pl.onAcc, 0); }
    }
    // группа: тёмная схема + палитра
    { RectF g1 = rG1(); FillRR(g, g1, R, Pl.card);
      float dis = MY ? 0.f : .55f;
      if (hot == H_TG + T_DARK && MY) RowHover(g, Pl, g1, R, rTg(T_DARK));
      ToggleRow(g, Pl, T_DARK, dis);
      { Pen lp(Pl.line, 1.f); g.DrawLine(&lp, 36.f, 216.f, 344.f, 216.f); }
      Tx(g, L"Палитра из обоев", sB, RectF(36, 216, 150, 40), Pl.text);
      static const int idx[5] = { 2, 5, 0, 1, 3 };
      for (int i = 0; i < 5; i++) { float cx = 192 + i * 32.f; SolidBrush b(pc(idx[i])); g.FillEllipse(&b, cx, 224.f, 24.f, 24.f); Pen op(Pl.line, 1.f); g.DrawEllipse(&op, cx, 224.f, 24.f, 24.f); }
    }
    // ---- акцентный цвет (Material You) ----
    Header(g, Pl, 266, L"Акцентный цвет", L"АКЦЕНТНЫЙ ЦВЕТ");
    { RectF ag = rAccG(); FillRR(g, ag, R, Pl.card); float dis = MY ? 0.f : .55f; int cur = AccGet();
      for (int i = 0; i < 7; i++) {
          RectF r = rAcc(i); Color c = i == 0 ? ThemeSeed(ThemeWpHue()) : ThemeSeed((float)ACCS[i - 1]); c = Mix(c, Pl.card, dis);
          if (hot == H_ACC + i && MY) { Pen hp(Mix(Pl.card, Pl.text, .25f), 2.f); g.DrawEllipse(&hp, r.X - 3, r.Y - 3, r.Width + 6, r.Height + 6); }
          SolidBrush b(c); g.FillEllipse(&b, r.X, r.Y, r.Width, r.Height);
          if (i == 0) { Pen dp(Mix(Pl.card, Pl.text, .6f), 1.5f); dp.SetDashStyle(DashStyleDot); g.DrawEllipse(&dp, r.X + 3.f, r.Y + 3.f, r.Width - 6, r.Height - 6); }
          if (cur == i) { Pen sp(Mix(Pl.acc, Pl.card, dis), 2.f); g.DrawEllipse(&sp, r.X - 3, r.Y - 3, r.Width + 6, r.Height + 6); }
      }
      Tx(g, cur == 0 ? L"из обоев" : L"свой цвет", sS, RectF(W - 160, 266, 132, 22), Pl.sub, StringAlignmentFar);
    }
    // ---- остров ----
    Header(g, Pl, 356, L"Остров", L"ОСТРОВ");
    { RectF g3 = rG3(); FillRR(g, g3, R, Pl.card);
      for (int t = T_AUTO; t <= T_LITE; t++) if (hot == H_TG + t) RowHover(g, Pl, g3, R, rTg(t));
      Pen lp(Pl.line, 1.f);
      for (int t = T_AUTO; t <= T_LITE; t++) { RectF r = rTg(t); if (t > T_AUTO) g.DrawLine(&lp, 36.f, r.Y, 344.f, r.Y); ToggleRow(g, Pl, t, 0); }
    }
    // ---- функции ----
    Header(g, Pl, 600, L"Функции", L"ФУНКЦИИ");
    { RectF g2 = rG2(); FillRR(g, g2, R, Pl.card);
      for (int t = T_WX; t <= T_HUD; t++) if (hot == H_TG + t) RowHover(g, Pl, g2, R, rTg(t));
      Pen lp(Pl.line, 1.f);
      for (int t = T_WX; t <= T_HUD; t++) { RectF r = rTg(t); if (t > T_WX) g.DrawLine(&lp, 36.f, r.Y, 344.f, r.Y); ToggleRow(g, Pl, t, 0); }
      g.DrawLine(&lp, 36.f, 624.f + 4 * 42, 344.f, 624.f + 4 * 42);
      Tx(g, L"Город для погоды", sB, RectF(36, 624.f + 4 * 42, 150, 42), Pl.text);
      RectF f = rField(); FillRR(g, f, MY ? 14.f : 9.f, Pl.field);
      if (cityFocus) StrokeRR(g, RectF(f.X + 1, f.Y + 1, f.Width - 2, f.Height - 2), MY ? 13.f : 8.f, Pl.acc, 2.f);
    }
    // ---- сегменты ----
    static const wchar_t* HS[S_N] = { L"Размер острова", L"Скругление", L"Отступ сверху, px", L"Автоскрытие через" };
    static const wchar_t* HU[S_N] = { L"РАЗМЕР ОСТРОВА", L"СКРУГЛЕНИЕ", L"ОТСТУП СВЕРХУ, PX", L"АВТОСКРЫТИЕ ЧЕРЕЗ" };
    for (int s = 0; s < S_N; s++) { Header(g, Pl, 844 + s * 68.f, HS[s], HU[s]); SegRow(g, Pl, s, s == S_DELAY && !s_auto ? .55f : 0.f); }
    g.ResetClip(); g.ResetTransform(); g.ScaleTransform(SS, SS);

    // ===== шапка (не прокручивается) =====
    { SolidBrush b(Pl.bg); g.FillRectangle(&b, 0.f, 0.f, W, 60.f); }
    if (scr > .5f) { Pen lp(Pl.line, 1.f); g.DrawLine(&lp, 0.f, 59.5f, W, 59.5f); }
    Tx(g, L"Настройки", sT, RectF(24, 12, 260, 30), Pl.text);
    Tx(g, L"Dynamic Island", sS, RectF(25, 38, 260, 18), Pl.sub);
    { RectF c = rClose(); if (hot == H_CLOSE) { SolidBrush b(Pl.card); g.FillEllipse(&b, c.X, c.Y, c.Width, c.Height); }
      DrawIcon(g, IC_CLOSE, c.X + 16, c.Y + 16, 18, hot == H_CLOSE ? Pl.text : Pl.sub, IS); }
    // полоса прокрутки
    if (MaxScroll() > 1) {
        float tr = WH - 60 - 16, th = max(36.f, tr * (WH - 60) / (H - 60)), ty = 68 + (scr / MaxScroll()) * (tr - th);
        FillRR(g, RectF(W - 7, ty, 4, th), 2.f, Mix(Pl.bg, Pl.text, .28f));
    }
    { Pen bp(Pl.line, 1.f); g.DrawRectangle(&bp, .5f, .5f, W - 1, WH - 1); }

    Graphics gd(dc); gd.DrawImage(&bmp, Rect(0, 0, Wp, Hp), 0, 0, Wp, Hp, UnitPixel);
}

// ---- логика ----
static void Apply() { g_themeDirty = true; SaveSettings(); if (hs) InvalidateRect(hs, 0, FALSE); }
static void CommitCity() {
    if (!hEdit) return; wchar_t b[64] = { 0 }; GetWindowTextW(hEdit, b, 64);
    bool ch = false; { std::lock_guard<std::mutex> lk(cm); if (b[0] && s_city != b) { s_city = b; ch = true; } }
    if (ch) { g_wxReset = true; SaveSettings(); }
}
static void UpdateEdit() {   // поле города двигается вместе с прокруткой и прячется, когда уезжает из видимой области
    if (!hEdit) return; RectF f = rField(); float y = f.Y - scr; bool vis = y >= 60 && y + f.Height <= WH;
    SetWindowPos(hEdit, 0, P(f.X + 12), P(y + 7), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (vis != editVis) { ShowWindow(hEdit, vis ? SW_SHOWNA : SW_HIDE); editVis = vis; }
}
static int HitAt(float x, float y) {
    if (In(rClose(), x, y)) return H_CLOSE;
    if (y < 60 || y > WH) return H_NONE;
    y += scr;
    for (int i = 0; i < 2; i++) if (In(rCard(i), x, y)) return H_CARD + i;
    if (s_theme != 0) for (int i = 0; i < 7; i++) { RectF r = rAcc(i); if (In(RectF(r.X - 5, r.Y - 5, r.Width + 10, r.Height + 10), x, y)) return H_ACC + i; }
    if (s_theme != 0 && In(rTg(T_DARK), x, y)) return H_TG + T_DARK;
    for (int t = T_AUTO; t < T_N; t++) if (In(rTg(t), x, y)) return H_TG + t;
    for (int s = 0; s < S_N; s++) for (int i = 0; i < SEGN[s]; i++) if (In(rSeg(s, i), x, y)) return H_SEG + s * 8 + i;
    return H_NONE;
}
static void Click(int h) {
    if (h == H_CLOSE) { CommitCity(); ShowWindow(hs, SW_HIDE); return; }
    if (h >= H_CARD && h < H_CARD + 2) { s_theme = h - H_CARD; Apply(); }
    else if (h >= H_ACC && h < H_ACC + 7) { s_accent = h == H_ACC ? -1 : ACCS[h - H_ACC - 1]; Apply(); }
    else if (h >= H_TG && h < H_TG + T_N) { std::atomic<int>* f = Flag(h - H_TG); *f = *f ? 0 : 1; if (h - H_TG == T_WX) g_wxReset = true; Apply(); }
    else if (h >= H_SEG) { int s = (h - H_SEG) / 8, i = (h - H_SEG) % 8; if (s < S_N && i < SEGN[s]) { SegSet(s, i); Apply(); } }
}

static LRESULT CALLBACK SProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_NCHITTEST: {
        POINT pt{ (int)(short)LOWORD(l), (int)(short)HIWORD(l) }; ScreenToClient(h, &pt);
        float x = pt.x / SS, y = pt.y / SS; if (y >= 0 && y < 60 && !In(rClose(), x, y)) return HTCAPTION;
        return HTCLIENT; }
    case WM_MOUSEMOVE: {
        if (!tracking) { TRACKMOUSEEVENT te{ sizeof te, TME_LEAVE, h, 0 }; TrackMouseEvent(&te); tracking = true; }
        int n = HitAt((short)LOWORD(l) / SS, (short)HIWORD(l) / SS); if (n != hot) { hot = n; InvalidateRect(h, 0, FALSE); }
        return 0; }
    case WM_MOUSELEAVE: tracking = false; if (hot != H_NONE) { hot = H_NONE; InvalidateRect(h, 0, FALSE); } return 0;
    case WM_MOUSEWHEEL: { float d = (short)HIWORD(w) / 120.f; scrT = cl(scrT - d * 64.f, 0, MaxScroll()); return 0; }
    case WM_SETCURSOR: if (LOWORD(l) == HTCLIENT && hot != H_NONE) { SetCursor(LoadCursor(0, IDC_HAND)); return TRUE; } break;
    case WM_LBUTTONDOWN: { SetFocus(h); int n = HitAt((short)LOWORD(l) / SS, (short)HIWORD(l) / SS); if (n != H_NONE) Click(n); return 0; }
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) { CommitCity(); SetFocus(h); }
        else if (LOWORD(w) == IDCANCEL) { CommitCity(); ShowWindow(h, SW_HIDE); }
        else if (LOWORD(w) == ID_CITY) {
            if (HIWORD(w) == EN_SETFOCUS) { cityFocus = true; InvalidateRect(h, 0, FALSE); }
            else if (HIWORD(w) == EN_KILLFOCUS) { cityFocus = false; CommitCity(); InvalidateRect(h, 0, FALSE); }
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        Pal Pl = GetPal(); COLORREF c = CR(Pl.field);
        if (!editBr || c != editCol) { if (editBr) DeleteObject(editBr); editBr = CreateSolidBrush(c); editCol = c; }
        SetTextColor((HDC)w, CR(Pl.text)); SetBkColor((HDC)w, c); return (LRESULT)editBr; }
    case WM_TIMER: {
        if (!IsWindowVisible(h)) return 0;
        bool ch = false;
        for (int i = 0; i < T_N; i++) {
            float t = *Flag(i) ? 1.f : 0.f, d = t - tg[i];
            if (fabsf(d) < .01f) { if (tg[i] != t) { tg[i] = t; ch = true; } } else { tg[i] += d * .3f; ch = true; }
        }
        for (int s = 0; s < S_N; s++) { float t = (float)SegGet(s), d = t - segA[s]; if (fabsf(d) < .01f) { if (segA[s] != t) { segA[s] = t; ch = true; } } else { segA[s] += d * .3f; ch = true; } }
        { float d = scrT - scr; if (fabsf(d) > .3f) { scr += d * .35f; ch = true; UpdateEdit(); } else if (scr != scrT) { scr = scrT; ch = true; UpdateEdit(); } }
        DWORD sig = TC(0, 1).GetValue() * 31u ^ TC(2, 1).GetValue() * 17u ^ TC(5, 1).GetValue() ^ (DWORD)s_theme.load() ^ ((DWORD)s_dark.load() << 5);
        if (sig != lastSig) { lastSig = sig; ch = true; if (hEdit) InvalidateRect(hEdit, 0, TRUE); }
        if (ch) InvalidateRect(h, 0, FALSE);
        return 0; }
    case WM_CLOSE: CommitCity(); ShowWindow(h, SW_HIDE); return 0;
    }
    return DefWindowProc(h, m, w, l);
}

void OpenSettings() {
    if (!hs) {
        WNDCLASSW wc{}; wc.lpfnWndProc = SProc; wc.hInstance = GetModuleHandle(0); wc.lpszClassName = L"WinIslandSet"; wc.style = CS_DROPSHADOW;
        wc.hCursor = LoadCursor(0, IDC_ARROW); RegisterClassW(&wc);
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        SS = g_dpi; WH = min(H, floorf(((wa.bottom - wa.top) - 24.f) / SS)); if (WH < 420) { WH = 420; SS = min(SS, ((wa.bottom - wa.top) - 24.f) / WH); }
        int ww = P(W), wh = P(WH);
        hs = CreateWindowExW(WS_EX_APPWINDOW, L"WinIslandSet", L"Dynamic Island — Настройки", WS_POPUP | WS_SYSMENU | WS_CLIPCHILDREN,
            wa.left + ((wa.right - wa.left) - ww) / 2, wa.top + ((wa.bottom - wa.top) - wh) / 2, ww, wh, 0, 0, GetModuleHandle(0), 0);
        int pref = 2; DwmSetWindowAttribute(hs, 33, &pref, sizeof pref);       // Win11: скруглённые углы (на Win10 игнорируется)
        RectF f = rField();
        hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, P(f.X + 12), P(f.Y + 7), P(f.Width - 24), P(19), hs, (HMENU)(INT_PTR)ID_CITY, GetModuleHandle(0), 0);
        SendMessage(hEdit, EM_LIMITTEXT, 60, 0);
        BuildFonts();
        SetTimer(hs, 1, 16, 0);
    }
    for (int i = 0; i < T_N; i++) tg[i] = *Flag(i) ? 1.f : 0.f;
    for (int s = 0; s < S_N; s++) segA[s] = (float)SegGet(s);
    scr = scrT = 0; editVis = true; ShowWindow(hEdit, SW_SHOWNA); UpdateEdit();
    SetWindowTextW(hEdit, GetCity().c_str());
    ShowWindow(hs, SW_SHOW); SetForegroundWindow(hs); InvalidateRect(hs, 0, FALSE);
}
bool SettingsOpen() { return hs && IsWindowVisible(hs); }
bool SettingsDialogMsg(MSG* m) { return hs && IsWindowVisible(hs) && IsDialogMessage(hs, m); }
