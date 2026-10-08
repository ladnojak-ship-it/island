#include "render.h"
#include "settings.h"
#include "theme.h"
#include "anim.h"

int g_canvasLeft = 0;
static HWND hw; static HDC mdc; static void* bits; static Bitmap* bmp; static Graphics* G;
static Font *fT, *fA, *fS, *fC, *fI, *fB, *fSym;
static StringFormat *sfN, *sfC, *sfF;
static TextureBrush* artBrush = nullptr; static Bitmap* artFor = nullptr;

static StringFormat* MkSF(StringAlignment a) {
    StringFormat* f = new StringFormat(StringFormatFlagsNoWrap);
    f->SetAlignment(a); f->SetLineAlignment(StringAlignmentCenter); f->SetTrimming(StringTrimmingEllipsisCharacter); return f;
}
bool RenderInit(HWND h) {
    hw = h; int Wp = (int)(CW * g_S), Hp = (int)(CH * g_S);
    g_canvasLeft = (GetSystemMetrics(SM_CXSCREEN) - Wp) / 2;
    BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), Wp, -Hp, 1, 32, BI_RGB };
    HBITMAP hb = CreateDIBSection(0, &bi, DIB_RGB_COLORS, &bits, 0, 0); if (!hb) return false;
    mdc = CreateCompatibleDC(0); SelectObject(mdc, hb);
    bmp = new Bitmap(Wp, Hp, Wp * 4, PixelFormat32bppPARGB, (BYTE*)bits);
    G = new Graphics(bmp); G->SetSmoothingMode(SmoothingModeAntiAlias); G->SetTextRenderingHint(TextRenderingHintAntiAlias);
    fT = new Font(L"Segoe UI", 15.f, FontStyleBold, UnitPixel); fA = new Font(L"Segoe UI", 13.f, FontStyleRegular, UnitPixel);
    fS = new Font(L"Segoe UI", 11.f, FontStyleRegular, UnitPixel); fC = new Font(L"Segoe UI", 13.f, FontStyleBold, UnitPixel);
    fI = new Font(L"Segoe MDL2 Assets", 17.f, FontStyleRegular, UnitPixel); fB = new Font(L"Segoe UI", 30.f, FontStyleRegular, UnitPixel);
    fSym = new Font(L"Segoe UI Symbol", 18.f, FontStyleRegular, UnitPixel);
    sfN = MkSF(StringAlignmentNear); sfC = MkSF(StringAlignmentCenter); sfF = MkSF(StringAlignmentFar);
    return true;
}

Lay LayoutFor(float pw) {
    Lay l; l.px = (CW - pw) / 2; l.py = 8; l.pw = pw; float x = l.px, y = l.py, cx = x + pw / 2;
    l.prog = RectF(x + 62, y + 98, pw - 124, 4); l.vb = RectF(x + 54, y + 174, pw - 118, 5);
    l.prev = PointF(cx - 72, y + 134); l.play = PointF(cx, y + 134); l.next = PointF(cx + 72, y + 134);
    for (int i = 0; i < 5; i++) l.launch[i] = PointF(cx + (i - 2) * 46, y + 212);
    return l;
}

// ---------- мелкие помощники ----------
static void RR(GraphicsPath& p, RectF r, float rad) {
    rad = min(rad, min(r.Width, r.Height) / 2); float d = rad * 2;
    if (d <= 0) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90); p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}
static void Txt(Graphics& g, const wchar_t* s, Font* f, RectF r, Color c, StringAlignment a = StringAlignmentNear) {
    SolidBrush b(c); g.DrawString(s, -1, f, r, a == StringAlignmentNear ? sfN : a == StringAlignmentCenter ? sfC : sfF, &b);
}
static void Glyph(Graphics& g, const wchar_t* s, float cx, float cy, Color c) { Txt(g, s, fI, RectF(cx - 20, cy - 14, 40, 28), c, StringAlignmentCenter); }
static void Bar(Graphics& g, float x, float y, float w, float h, float fr, Color bg, Color fg) {
    GraphicsPath p; RR(p, RectF(x, y, w, h), h / 2); SolidBrush b(bg); g.FillPath(&b, &p);
    if (fr > 0.001f) { GraphicsPath q; RR(q, RectF(x, y, max(h, w * fr), h), h / 2); SolidBrush f(fg); g.FillPath(&f, &q); }
}
static void EQ(Graphics& g, float x, float cy, float amp, double t, float mh, Color c) {
    SolidBrush b(c);
    for (int i = 0; i < 4; i++) {
        float h = 4 + (mh - 4) * amp * (0.5f + 0.5f * sinf((float)t * (5 + i * 1.9f) + i * 1.3f));
        GraphicsPath p; RR(p, RectF(x + i * 5, cy - h / 2, 3, h), 1.5f); g.FillPath(&b, &p);
    }
}
static void ScaleAt(Graphics& g, float cx, float cy, float s) { g.TranslateTransform(cx, cy); g.ScaleTransform(s, s); g.TranslateTransform(-cx, -cy); }
static float stag(float a, int i) { return ease((a - i * .07f) / .5f); }   // ступенчатое появление рядов
static Color HSV(float h, float s, float v) {
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c, r = 0, g = 0, b = 0;
    switch ((int)(h / 60) % 6) { case 0: r = c; g = x; break; case 1: r = x; g = c; break; case 2: g = c; b = x; break;
        case 3: g = x; b = c; break; case 4: r = x; b = c; break; default: r = c; b = x; }
    return Color(255, (BYTE)((r + m) * 255), (BYTE)((g + m) * 255), (BYTE)((b + m) * 255));
}
static std::wstring Tm(double s) { if (s < 0) s = 0; wchar_t b[16]; swprintf(b, 16, L"%d:%02d", (int)s / 60, (int)s % 60); return b; }

// бегущая строка для длинных названий (ширина текста кэшируется)
static void Marquee(Graphics& g, int slot, const std::wstring& s, Font* f, RectF r, Color c, double t, float xoff) {
    static std::wstring tt[2]; static float tw[2];
    if (tt[slot] != s) { tt[slot] = s; RectF b; g.MeasureString(s.c_str(), -1, f, PointF(0, 0), sfN, &b); tw[slot] = b.Width; }
    float w = tw[slot], off = 0;
    if (w > r.Width + 1) {
        float over = w - r.Width + 8, travel = over / 32.f, cyc = 2.4f + 2 * travel, ph = (float)fmod(t, cyc);
        off = ph < 1.2f ? 0 : ph < 1.2f + travel ? (ph - 1.2f) * 32.f : ph < 2.4f + travel ? over : over - (ph - 2.4f - travel) * 32.f;
    }
    GraphicsState st = g.Save(); g.SetClip(r, CombineModeIntersect);
    SolidBrush b(c); g.DrawString(s.c_str(), -1, f, RectF(r.X + xoff - off, r.Y, max(w + 16, r.Width), r.Height), sfN, &b);
    g.Restore(st);
}

static const wchar_t* LG[5] = { L"\uE8B7", L"\uE756", L"\uE8EF", L"\uE722", L"\uE713" };   // проводник, терминал, калькулятор, скриншот, настройки

// ================= кадр =================
void RenderFrame(const View& v) {
    Graphics& g = *G; const bool lite = s_lite != 0; const Media& m = v.m;
    float pw = max(v.pw, 24.f), ph = max(v.ph, 24.f);
    Lay L = LayoutFor(pw); float px = L.px, py = L.py, rad = min(ph / 2, 34.f);

    // грязный прямоугольник: рисуем и обновляем окно только вокруг пилюли (быстро)
    float mg = lite ? 4.f : 18.f;
    int x0 = (int)floorf(max(0.f, px - mg)), x1 = (int)ceilf(min((float)CW, px + pw + mg)), y1 = (int)ceilf(min((float)CH, py + ph + mg + 6));
    int Wp = (int)(CW * g_S), Hp = (int)(CH * g_S);
    int sx0 = (int)(x0 * g_S), sw = min(Wp - sx0, (int)ceilf((x1 - x0) * g_S) + 1), sh = min(Hp, (int)ceilf(y1 * g_S) + 1);
    g.ResetTransform(); g.ResetClip();
    g.SetCompositingMode(CompositingModeSourceCopy);
    { SolidBrush clr(Color(0, 0, 0, 0)); g.FillRectangle(&clr, sx0, 0, sw, sh); }
    g.SetCompositingMode(CompositingModeSourceOver);
    g.ScaleTransform(g_S, g_S);

    if (v.art != artFor) { delete artBrush; artBrush = v.art ? new TextureBrush(v.art, WrapModeClamp) : nullptr; artFor = v.art; }
    const bool hasArt = m.has && artBrush && s_art;

    if (!lite) for (int i = 3; i >= 1; i--) {      // мягкая тень
        GraphicsPath sp; RR(sp, RectF(px - i * 2.5f, py + i * 2, pw + i * 5, ph + i * 4), rad + i * 2.5f);
        SolidBrush sb(Color(11, 0, 0, 0)); g.FillPath(&sb, &sp);
    }
    GraphicsPath path; RR(path, RectF(px, py, pw, ph), rad);
    { SolidBrush bk(TC(0, 1)); g.FillPath(&bk, &path); }
    if (!lite && v.glow > .01f) {                  // «живое» свечение, пока играет музыка
        Pen p1(TC(2, .08f * v.glow), 6.f), p2(TC(2, .45f * v.glow), 1.5f); g.DrawPath(&p1, &path); g.DrawPath(&p2, &path);
    }
    g.SetClip(&path);

    float ae = cl((v.fade - .3f) / .7f, 0, 1), ca = cl(1 - v.fade * 2.5f, 0, 1);
    unsigned hh = 5381; for (wchar_t c : m.title) hh = hh * 33 + c;
    Color c1 = HSV((float)(hh % 360), .65f, .95f), c2 = HSV((float)((hh + 40) % 360), .8f, .5f);

    auto Art = [&](RectF r, float rd, float a) {       // обложка / заглушка с градиентом
        GraphicsPath p; RR(p, r, rd);
        if (hasArt) {
            artBrush->ResetTransform(); artBrush->ScaleTransform(r.Width / 128, r.Height / 128); artBrush->TranslateTransform(r.X, r.Y, MatrixOrderAppend);
            g.FillPath(artBrush, &p); if (a < 1) { SolidBrush ov(TC(0, 1 - a)); g.FillPath(&ov, &p); }
        } else if (m.has) {
            Color a1 = s_theme == 0 ? Al(c1, a) : TC(2, a), a2 = s_theme == 0 ? Al(c2, a) : TC(5, a);
            LinearGradientBrush lb(PointF(r.X, r.Y), PointF(r.X + r.Width, r.Y + r.Height), a1, a2); g.FillPath(&lb, &p);
        } else { SolidBrush sb(TC(1, a * .12f)); g.FillPath(&sb, &p); }
    };

    // ---------- компактный режим и всплывашки ----------
    if (ca > 0.01f) {
        float cy = py + ph / 2; wchar_t b[48];
        GraphicsState st = g.Save();
        if (v.hud) ScaleAt(g, px + 24, cy, .6f + .4f * v.hudPop);   // иконка «выпрыгивает»
        if (v.hud == 1) {
            Glyph(g, (v.mute || v.volDisp < .005f) ? L"\uE74F" : L"\uE767", px + 24, cy, TC(1, ca));
            g.Restore(st);
            Bar(g, px + 46, cy - 2.5f, pw - 46 - 60, 5, v.mute ? 0 : v.volDisp, TC(1, ca * .25f), TC(2, ca));
            swprintf(b, 48, L"%d", (int)(v.volDisp * 100 + .5f));
            Txt(g, b, fC, RectF(px + pw - 52, cy - 10, 36, 20), TC(1, ca), StringAlignmentFar);
        } else if (v.hud == 2) {
            Color gc = Color((BYTE)(ca * 255), 48, 209, 88);
            Glyph(g, L"\uE83F", px + 24, cy, v.sys.ac ? gc : TC(1, ca)); g.Restore(st);
            Txt(g, v.sys.ac ? L"Зарядка" : L"От батареи", fC, RectF(px + 44, cy - 10, 150, 20), TC(1, ca));
            swprintf(b, 48, L"%d%%", v.sys.bat);
            Txt(g, b, fC, RectF(px + pw - 70, cy - 10, 52, 20), v.sys.ac ? gc : TC(1, ca), StringAlignmentFar);
        } else if (v.hud == 3) {
            Txt(g, L"\u2602", fSym, RectF(px + 10, cy - 14, 28, 28), TC(2, ca), StringAlignmentCenter); g.Restore(st);
            Txt(g, v.hudText.c_str(), fC, RectF(px + 46, cy - 10, pw - 62, 20), TC(1, ca));
        } else if (v.hud == 4) {
            Glyph(g, L"\uE715", px + 24, cy, TC(2, ca)); g.Restore(st);
            Txt(g, v.hudText.c_str(), fC, RectF(px + 46, cy - 10, pw - 62, 20), TC(1, ca));
        } else if (m.has) {
            g.Restore(st); st = g.Save(); ScaleAt(g, px + 23, cy, .8f + .2f * backOut(v.trackAnim));
            Art(RectF(px + 12, cy - 11, 22, 22), 6, ca); g.Restore(st);
            EQ(g, px + pw - 34, cy, v.eqAmp, v.now, 16, TC(2, ca));
        } else {
            g.Restore(st);
            SYSTEMTIME t; GetLocalTime(&t); swprintf(b, 48, L"%02d:%02d", t.wHour, t.wMinute);
            Txt(g, b, fC, RectF(px, cy - 10, pw, 20), TC(1, ca), StringAlignmentCenter);
        }
    }

    // ---------- раскрытый режим: ряды появляются по очереди ----------
    if (ae > 0.01f) {
        double pos = m.pos + (m.playing ? (GetTickCount64() - m.t) / 1000.0 : 0); if (m.dur > 0 && pos > m.dur) pos = m.dur;
        std::wstring ttl = m.has ? m.title : L"Ничего не играет", art = m.has ? m.artist : L"Запусти музыку";
        wchar_t b[200];

        { // ряд 0: обложка, название, эквалайзер
            float a = stag(ae, 0), ta = ease(v.trackAnim), tb = ease(cl(v.trackAnim * 1.3f - .3f, 0, 1));
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            { GraphicsState s2 = g.Save(); ScaleAt(g, px + 52, py + 50, .8f + .2f * backOut(v.trackAnim));
              Art(RectF(px + 20, py + 18, 64, 64), 14, a);
              if (!hasArt) Txt(g, L"\u266A", fB, RectF(px + 20, py + 18, 64, 64), m.has ? Al(Color(255, 255, 255, 255), a) : TC(1, a * .5f), StringAlignmentCenter);
              g.Restore(s2); }
            Marquee(g, 0, ttl, fT, RectF(px + 98, py + 22, pw - 98 - 64, 22), TC(1, a * ta), v.now, (1 - ta) * 16);
            Marquee(g, 1, art, fA, RectF(px + 98, py + 45, pw - 98 - 64, 20), TC(1, a * .6f * tb), v.now, (1 - tb) * 16);
            EQ(g, px + pw - 44, py + 32, v.eqAmp, v.now, 20, TC(2, a));
            g.Restore(st);
        }
        { // ряд 1: прогресс (при наведении полоса растёт, появляется ползунок)
            float a = stag(ae, 1), hv = v.hov[8], hgt = 4 + 3 * hv, cy = L.prog.Y + 2, fr = m.dur > 0 ? (float)(pos / m.dur) : 0;
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            Bar(g, L.prog.X, cy - hgt / 2, L.prog.Width, hgt, fr, TC(1, a * .22f), TC(2, a));
            if (hv > .02f) { float r = 6 * hv, tx = L.prog.X + L.prog.Width * fr; SolidBrush tbr(TC(1, a)); g.FillEllipse(&tbr, tx - r, cy - r, 2 * r, 2 * r); }
            Txt(g, Tm(pos).c_str(), fS, RectF(px + 18, L.prog.Y - 8, 42, 20), TC(1, a * .55f));
            Txt(g, Tm(m.dur).c_str(), fS, RectF(px + pw - 60, L.prog.Y - 8, 42, 20), TC(1, a * .55f), StringAlignmentFar);
            g.Restore(st);
        }
        { // ряд 2: кнопки управления (наведение + «нажатие» + морфинг play/pause)
            float a = stag(ae, 2);
            for (int i = 0; i < 3; i++) {
                PointF c = i == 0 ? L.prev : i == 1 ? L.play : L.next;
                GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
                ScaleAt(g, c.X, c.Y, (1 + .08f * v.hov[i]) * (1 - .14f * v.press[i]));
                if (i == 1) {
                    SolidBrush wb(TC(3, a)); g.FillEllipse(&wb, c.X - 22, c.Y - 22, 44.f, 44.f);
                    Glyph(g, L"\uE768", c.X, c.Y, TC(4, a * (1 - v.playAnim)));
                    Glyph(g, L"\uE769", c.X, c.Y, TC(4, a * v.playAnim));
                } else {
                    if (v.hov[i] > .02f) { SolidBrush hb(TC(1, a * .14f * v.hov[i])); g.FillEllipse(&hb, c.X - 22, c.Y - 22, 44.f, 44.f); }
                    Glyph(g, i == 0 ? L"\uE892" : L"\uE893", c.X, c.Y, TC(1, a));
                }
                g.Restore(st);
            }
        }
        { // ряд 3: громкость
            float a = stag(ae, 3), hv = v.hov[9], hgt = 5 + 3 * hv, cy = L.vb.Y + 2.5f;
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            Glyph(g, (v.mute || v.volDisp < .005f) ? L"\uE74F" : L"\uE767", px + 30, cy, TC(1, a * .8f));
            Bar(g, L.vb.X, cy - hgt / 2, L.vb.Width, hgt, v.mute ? 0 : v.volDisp, TC(1, a * .22f), TC(1, a));
            if (hv > .02f) { float r = 6 * hv, tx = L.vb.X + L.vb.Width * (v.mute ? 0 : v.volDisp); SolidBrush tbr(TC(1, a)); g.FillEllipse(&tbr, tx - r, cy - r, 2 * r, 2 * r); }
            swprintf(b, 200, L"%d", (int)(v.volDisp * 100 + .5f));
            Txt(g, b, fS, RectF(px + pw - 58, cy - 10, 38, 20), TC(1, a * .55f), StringAlignmentFar);
            g.Restore(st);
        }
        if (s_launch) for (int i = 0; i < 5; i++) {   // ряд 4: кнопки запуска, каждая со своей задержкой
            float a = stag(ae, 3 + i); PointF c = L.launch[i];
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            ScaleAt(g, c.X, c.Y, (1 + .12f * v.hov[3 + i]) * (1 - .14f * v.press[3 + i]));
            SolidBrush cb(TC(5, a)); g.FillEllipse(&cb, c.X - 17, c.Y - 17, 34.f, 34.f);
            Glyph(g, LG[i], c.X, c.Y, TC(1, a)); g.Restore(st);
        }
        { // ряд 5: погода и системная информация
            float a = stag(ae, 6); SYSTEMTIME t; GetLocalTime(&t); int n = 0;
            if (s_wx && v.wx.ok) n = swprintf(b, 200, L"%ls %d°    ", WxSym(v.wx.code), v.wx.temp);
            if (v.sys.bat >= 0) swprintf(b + n, 200 - n, L"CPU %d%%    RAM %d%%    BAT %d%%%ls    %02d:%02d", (int)(v.sys.cpu + .5f), v.sys.ram, v.sys.bat, v.sys.ac ? L" \u26A1" : L"", t.wHour, t.wMinute);
            else swprintf(b + n, 200 - n, L"CPU %d%%    RAM %d%%    %02d:%02d", (int)(v.sys.cpu + .5f), v.sys.ram, t.wHour, t.wMinute);
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            Txt(g, b, fS, RectF(px, py + (s_launch ? 232 : 194), pw, 18), TC(1, a * .5f), StringAlignmentCenter); g.Restore(st);
        }
    }
    g.ResetClip();

    POINT dst{ g_canvasLeft + sx0, 0 }, src{ sx0, 0 }; SIZE sz{ sw, sh };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hw, NULL, &dst, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
}
