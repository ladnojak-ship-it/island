#include "render.h"
#include "settings.h"
#include "theme.h"
#include "anim.h"
#include "fonts.h"
#include "icons.h"
#include "gfx.h"

int g_canvasLeft = 0;
std::atomic<bool> g_resizeReq{ false };
static HWND hw; static HDC mdc = nullptr; static HBITMAP hbm = nullptr; static void* bits; static Bitmap* bmp = nullptr; static Graphics* G = nullptr;
static int curW = 0, curH = 0;
static Font *fT = nullptr, *fA = nullptr, *fS = nullptr, *fC = nullptr;
static int fontTheme = -1, fontGen = 0;
static StringFormat *sfN, *sfC, *sfF, *sfM;
static TextureBrush* artBrush = nullptr; static Bitmap* artFor = nullptr;
static constexpr float ART_PX = 256.f;

static StringFormat* MkSF(StringAlignment a) {
    StringFormat* f = new StringFormat(StringFormatFlagsNoWrap);
    f->SetAlignment(a); f->SetLineAlignment(StringAlignmentCenter); f->SetTrimming(StringTrimmingEllipsisCharacter); return f;
}
static void BuildFonts() {
    int th = s_theme ? 1 : 0; if (th == fontTheme) return;
    delete fT; delete fA; delete fS; delete fC;
    fT = MkFont(th, 18.f, true); fA = MkFont(th, 14.5f, false); fS = MkFont(th, 13.f, false); fC = MkFont(th, 15.f, true);
    fontTheme = th; fontGen++;
}
static bool MakeSurface() {
    int w = (int)ceilf(CW * g_S), h = (int)ceilf(CH * g_S);
    delete G; G = nullptr; delete bmp; bmp = nullptr;
    BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB };
    HBITMAP nb = CreateDIBSection(0, &bi, DIB_RGB_COLORS, &bits, 0, 0); if (!nb) return false;
    if (!mdc) mdc = CreateCompatibleDC(0);
    SelectObject(mdc, nb); if (hbm) DeleteObject(hbm); hbm = nb; curW = w; curH = h;
    bmp = new Bitmap(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)bits);
    G = new Graphics(bmp);
    G->SetSmoothingMode(SmoothingModeHighQuality); G->SetPixelOffsetMode(PixelOffsetModeHighQuality);
    G->SetInterpolationMode(InterpolationModeHighQualityBicubic); G->SetCompositingQuality(CompositingQualityHighQuality);
    G->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);   // PARGB-слой: ClearType нельзя, берём серый с подгонкой под сетку
    g_canvasLeft = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    return true;
}
bool RenderInit(HWND h) {
    hw = h; if (!MakeSurface()) return false;
    sfN = MkSF(StringAlignmentNear); sfC = MkSF(StringAlignmentCenter); sfF = MkSF(StringAlignmentFar);
    sfM = StringFormat::GenericTypographic()->Clone(); sfM->SetLineAlignment(StringAlignmentCenter);
    BuildFonts(); return true;
}
bool RenderResize() {
    float sc = s_scale.load() / 100.f; g_S = g_dpi * sc;
    if (artBrush) { delete artBrush; artBrush = nullptr; artFor = nullptr; }
    return MakeSurface();
}

Lay LayoutFor(float pw) {
    Lay l; l.px = (CW - pw) / 2; l.py = 8; l.pw = pw; float x = l.px, y = l.py, cx = x + pw / 2;
    l.prog = RectF(x + 68, y + 102, pw - 136, 4); l.vb = RectF(x + 60, y + 180, pw - 130, 5);
    l.mute = PointF(x + 34, y + 182.5f);
    l.prev = PointF(cx - 76, y + 140); l.play = PointF(cx, y + 140); l.next = PointF(cx + 76, y + 140);
    for (int i = 0; i < 5; i++) l.launch[i] = PointF(cx + (i - 2) * 52, y + 222);
    return l;
}

// ---------- мелкие помощники ----------
static void Txt(Graphics& g, const wchar_t* s, Font* f, RectF r, Color c, StringAlignment a = StringAlignmentNear) {
    SolidBrush b(c); g.DrawString(s, -1, f, r, a == StringAlignmentNear ? sfN : a == StringAlignmentCenter ? sfC : sfF, &b);
}
static void Bar(Graphics& g, float x, float y, float w, float h, float fr, Color bg, Color fg) {
    GraphicsPath p; RR(p, RectF(x, y, w, h), h / 2); SolidBrush b(bg); g.FillPath(&b, &p);
    if (fr > 0.001f) { GraphicsPath q; RR(q, RectF(x, y, max(h, w * fr), h), h / 2); SolidBrush f(fg); g.FillPath(&f, &q); }
}
// Material 3 Expressive: «волнистый» прогресс. Пройденная часть — волна (амплитуда зависит от play), остаток — прямая линия
static void WaveBar(Graphics& g, float x, float cy, float w, float fr, float amp, double t, float th, Color bg, Color fg, float hv) {
    float xf = x + w * fr, gap = 4 + 2 * hv;
    Pen tp(bg, th); tp.SetStartCap(LineCapRound); tp.SetEndCap(LineCapRound);
    if (xf + gap < x + w) g.DrawLine(&tp, xf + gap, cy, x + w, cy);
    static PointF pts[200]; int n = 0; float end = max(x, xf - gap);
    for (float px = x; px <= end && n < 199; px += 2.5f) pts[n++] = PointF(px, cy + amp * sinf((px - x) * .42f - (float)t * 5.f));
    if (n >= 2) { Pen wp(fg, th); wp.SetStartCap(LineCapRound); wp.SetEndCap(LineCapRound); wp.SetLineJoin(LineJoinRound); g.DrawLines(&wp, pts, n); }
    else if (fr > .001f) { Pen wp(fg, th); wp.SetStartCap(LineCapRound); wp.SetEndCap(LineCapRound); g.DrawLine(&wp, x, cy, max(x + .1f, end), cy); }
    float hh = 18 + 4 * hv; GraphicsPath hp; RR(hp, RectF(xf - 2, cy - hh / 2, 4, hh), 2); SolidBrush hb(fg); g.FillPath(&hb, &hp);
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
    static std::wstring tt[2]; static float tw[2]; static int gen[2] = { -1, -1 };
    if (tt[slot] != s || gen[slot] != fontGen) { tt[slot] = s; gen[slot] = fontGen; RectF b; g.MeasureString(s.c_str(), -1, f, PointF(0, 0), sfN, &b); tw[slot] = b.Width; }
    float w = tw[slot], off = 0; bool scroll = w > r.Width + 1;
    if (scroll) {
        float over = w - r.Width + 8, travel = over / 32.f, cyc = 2.4f + 2 * travel, ph = (float)fmod(t, cyc);
        off = ph < 1.2f ? 0 : ph < 1.2f + travel ? (ph - 1.2f) * 32.f : ph < 2.4f + travel ? over : over - (ph - 2.4f - travel) * 32.f;
    }
    GraphicsState st = g.Save(); g.SetClip(r, CombineModeIntersect);
    if (scroll && off > 0) g.SetTextRenderingHint(TextRenderingHintAntiAlias);   // при движении — без подгонки к сетке (плавнее)
    SolidBrush b(c); g.DrawString(s.c_str(), -1, f, RectF(r.X + xoff - off, r.Y, max(w + 16, r.Width), r.Height), sfN, &b);
    g.Restore(st);
}

static int WxIcon(int c) {
    if (c == 0) return IC_SUN; if (c <= 3) return IC_CLOUD; if (c == 45 || c == 48) return IC_FOG;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return IC_SNOW; if (c >= 95) return IC_STORM; return IC_RAIN;
}
static const int LG[5] = { IC_FOLDER, IC_TERM, IC_CALC, IC_CAM, IC_GEAR };   // проводник, терминал, калькулятор, скриншот, настройки

// ================= кадр =================
void RenderFrame(const View& v) {
    BuildFonts();
    Graphics& g = *G; const bool lite = s_lite != 0; const bool MY = s_theme != 0; const Media& m = v.m;
    const int IS = MY ? 1 : 2;                                  // стиль иконок
    float pw = max(v.pw, 24.f), ph = max(v.ph, 24.f);
    Lay L = LayoutFor(pw); float px = L.px, py = L.py, rad = min(ph / 2, 34.f);
    if (MY && ph > 60) rad = min(rad, 36.f);

    // грязный прямоугольник: рисуем и обновляем окно только вокруг пилюли (быстро)
    float mg = lite ? 4.f : 18.f;
    int x0 = (int)floorf(max(0.f, px - mg)), x1 = (int)ceilf(min((float)CW, px + pw + mg)), y1 = (int)ceilf(min((float)CH, py + ph + mg + 6));
    int sx0 = (int)(x0 * g_S), sw = min(curW - sx0, (int)ceilf((x1 - x0) * g_S) + 1), sh = min(curH, (int)ceilf(y1 * g_S) + 1);
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
    { Pen edge(TC(1, MY ? .10f : .12f), 1.f); g.DrawPath(&edge, &path); }    // тонкий светлый кант — остров чётко читается на тёмных обоях
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
            artBrush->ResetTransform(); artBrush->ScaleTransform(r.Width / ART_PX, r.Height / ART_PX); artBrush->TranslateTransform(r.X, r.Y, MatrixOrderAppend);
            g.FillPath(artBrush, &p); if (a < 1) { SolidBrush ov(TC(0, 1 - a)); g.FillPath(&ov, &p); }
        } else if (m.has) {
            Color a1 = s_theme == 0 ? Al(c1, a) : TC(2, a), a2 = s_theme == 0 ? Al(c2, a) : TC(5, a);
            LinearGradientBrush lb(PointF(r.X, r.Y), PointF(r.X + r.Width, r.Y + r.Height), a1, a2); g.FillPath(&lb, &p);
        } else { SolidBrush sb(TC(MY ? 5 : 1, MY ? a : a * .12f)); g.FillPath(&sb, &p); }
    };

    // ---------- компактный режим и всплывашки ----------
    if (ca > 0.01f) {
        float cy = py + ph / 2; wchar_t b[48];
        GraphicsState st = g.Save();
        if (v.hud) ScaleAt(g, px + 26, cy, .6f + .4f * v.hudPop);   // иконка «выпрыгивает»
        if (v.hud == 1) {
            DrawIcon(g, (v.mute || v.volDisp < .005f) ? IC_MUTE : IC_VOL, px + 26, cy, 22, TC(1, ca), IS);
            g.Restore(st);
            Bar(g, px + 50, cy - (MY ? 3.f : 2.5f), pw - 50 - 62, MY ? 6.f : 5.f, v.mute ? 0 : v.volDisp, TC(1, ca * .25f), TC(2, ca));
            swprintf(b, 48, L"%d", (int)(v.volDisp * 100 + .5f));
            Txt(g, b, fC, RectF(px + pw - 56, cy - 11, 38, 22), TC(1, ca), StringAlignmentFar);
        } else if (v.hud == 2) {
            Color gc = Color((BYTE)(ca * 255), 48, 209, 88);
            DrawIcon(g, IC_BOLT, px + 26, cy, 22, v.sys.ac ? gc : TC(1, ca), IS); g.Restore(st);
            Txt(g, v.sys.ac ? L"Зарядка" : L"От батареи", fC, RectF(px + 46, cy - 11, 160, 22), TC(1, ca));
            swprintf(b, 48, L"%d%%", v.sys.bat);
            Txt(g, b, fC, RectF(px + pw - 76, cy - 11, 58, 22), v.sys.ac ? gc : TC(1, ca), StringAlignmentFar);
        } else if (v.hud == 3) {
            DrawIcon(g, IC_RAIN, px + 26, cy, 24, TC(2, ca), IS); g.Restore(st);
            Txt(g, v.hudText.c_str(), fC, RectF(px + 50, cy - 11, pw - 66, 22), TC(1, ca));
        } else if (v.hud == 4) {
            DrawIcon(g, IC_BELL, px + 26, cy, 22, TC(2, ca), IS); g.Restore(st);
            Txt(g, v.hudText.c_str(), fC, RectF(px + 50, cy - 11, pw - 66, 22), TC(1, ca));
        } else if (m.has) {
            g.Restore(st); st = g.Save(); ScaleAt(g, px + 24, cy, .8f + .2f * backOut(v.trackAnim));
            Art(RectF(px + 12, cy - 12, 24, 24), MY ? 8.f : 6.f, ca); g.Restore(st);
            EQ(g, px + pw - 36, cy, v.eqAmp, v.now, 18, TC(2, ca));
        } else {
            g.Restore(st);
            SYSTEMTIME t; GetLocalTime(&t); swprintf(b, 48, L"%02d:%02d", t.wHour, t.wMinute);
            Txt(g, b, fC, RectF(px, cy - 11, pw, 22), TC(1, ca), StringAlignmentCenter);
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
            { GraphicsState s2 = g.Save(); ScaleAt(g, px + 54, py + 52, .8f + .2f * backOut(v.trackAnim));
              Art(RectF(px + 20, py + 18, 68, 68), MY ? 20.f : 15.f, a);
              if (!hasArt) DrawIcon(g, IC_NOTE, px + 54, py + 52, 32, m.has ? Al(Color(255, 255, 255, 255), a) : TC(MY ? 2 : 1, a * .6f), IS);
              g.Restore(s2); }
            Marquee(g, 0, ttl, fT, RectF(px + 102, py + 20, pw - 102 - 66, 28), TC(1, a * ta), v.now, (1 - ta) * 16);
            Marquee(g, 1, art, fA, RectF(px + 102, py + 48, pw - 102 - 66, 24), TC(1, a * .62f * tb), v.now, (1 - tb) * 16);
            EQ(g, px + pw - 46, py + 34, v.eqAmp, v.now, 22, TC(2, a));
            g.Restore(st);
        }
        { // ряд 1: прогресс (при наведении полоса растёт, появляется ползунок)
            float a = stag(ae, 1), hv = v.hov[8], cy = L.prog.Y + 2, fr = m.dur > 0 ? (float)(pos / m.dur) : 0;
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            if (MY) WaveBar(g, L.prog.X, cy, L.prog.Width, fr, 2.4f * v.playAnim, v.now, 4.5f, TC(1, a * .22f), TC(2, a), hv);
            else {
                float hgt = 4 + 3 * hv;
                Bar(g, L.prog.X, cy - hgt / 2, L.prog.Width, hgt, fr, TC(1, a * .22f), TC(2, a));
                if (hv > .02f) { float r = 6 * hv, tx = L.prog.X + L.prog.Width * fr; SolidBrush tbr(TC(1, a)); g.FillEllipse(&tbr, tx - r, cy - r, 2 * r, 2 * r); }
            }
            Txt(g, Tm(pos).c_str(), fS, RectF(px + 18, L.prog.Y - 8, 46, 22), TC(1, a * .6f));
            Txt(g, Tm(m.dur).c_str(), fS, RectF(px + pw - 64, L.prog.Y - 8, 46, 22), TC(1, a * .6f), StringAlignmentFar);
            g.Restore(st);
        }
        { // ряд 2: кнопки управления (наведение + «нажатие» + морфинг play/pause)
            float a = stag(ae, 2);
            for (int i = 0; i < 3; i++) {
                PointF c = i == 0 ? L.prev : i == 1 ? L.play : L.next;
                GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
                ScaleAt(g, c.X, c.Y, (1 + .08f * v.hov[i]) * (1 - .14f * v.press[i]));
                if (i == 1) {
                    if (MY) {      // форма морфится: пауза -> круг, играет -> «сквиркл»
                        GraphicsPath bp; RR(bp, RectF(c.X - 27, c.Y - 23, 54, 46), 23 - 9 * v.playAnim);
                        SolidBrush wb(TC(3, a)); g.FillPath(&wb, &bp);
                    } else { SolidBrush wb(TC(3, a)); g.FillEllipse(&wb, c.X - 23, c.Y - 23, 46.f, 46.f); }
                    DrawIcon(g, IC_PLAY, c.X, c.Y, 28, TC(4, a * (1 - v.playAnim)), IS);
                    DrawIcon(g, IC_PAUSE, c.X, c.Y, 28, TC(4, a * v.playAnim), IS);
                } else {
                    if (v.hov[i] > .02f) {
                        SolidBrush hb(TC(1, a * .14f * v.hov[i]));
                        if (MY) { GraphicsPath hp; RR(hp, RectF(c.X - 24, c.Y - 22, 48, 44), 18); g.FillPath(&hb, &hp); }
                        else g.FillEllipse(&hb, c.X - 23, c.Y - 23, 46.f, 46.f);
                    }
                    DrawIcon(g, i == 0 ? IC_PREV : IC_NEXT, c.X, c.Y, 28, TC(1, a), IS);
                }
                g.Restore(st);
            }
        }
        { // ряд 3: громкость
            float a = stag(ae, 3), hv = v.hov[9], cy = L.vb.Y + 2.5f, vol = v.mute ? 0.f : v.volDisp;
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            DrawIcon(g, (v.mute || v.volDisp < .005f) ? IC_MUTE : IC_VOL, L.mute.X, cy, 22, TC(1, a * .85f), IS);
            if (MY) {
                float hgt = 8 + 2 * hv; Bar(g, L.vb.X, cy - hgt / 2, L.vb.Width, hgt, vol, TC(1, a * .22f), TC(2, a));
                float tx = L.vb.X + L.vb.Width * vol, hhh = 20 + 4 * hv; GraphicsPath hp; RR(hp, RectF(tx - 2, cy - hhh / 2, 4, hhh), 2);
                SolidBrush hb(TC(2, a)); Pen gp(TC(0, a), 2.f); g.DrawPath(&gp, &hp); g.FillPath(&hb, &hp);
            } else {
                float hgt = 5 + 3 * hv; Bar(g, L.vb.X, cy - hgt / 2, L.vb.Width, hgt, vol, TC(1, a * .22f), TC(1, a));
                if (hv > .02f) { float r = 6 * hv, tx = L.vb.X + L.vb.Width * vol; SolidBrush tbr(TC(1, a)); g.FillEllipse(&tbr, tx - r, cy - r, 2 * r, 2 * r); }
            }
            swprintf(b, 200, L"%d", (int)(v.volDisp * 100 + .5f));
            Txt(g, b, fS, RectF(px + pw - 62, cy - 11, 44, 22), TC(1, a * .6f), StringAlignmentFar);
            g.Restore(st);
        }
        if (s_launch) for (int i = 0; i < 5; i++) {   // ряд 4: кнопки запуска, каждая со своей задержкой
            float a = stag(ae, 3 + i); PointF c = L.launch[i];
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            ScaleAt(g, c.X, c.Y, (1 + .12f * v.hov[3 + i]) * (1 - .14f * v.press[3 + i]));
            SolidBrush cb(TC(5, a));
            if (MY) { GraphicsPath bp; RR(bp, RectF(c.X - 19, c.Y - 19, 38, 38), 13); g.FillPath(&cb, &bp); }
            else g.FillEllipse(&cb, c.X - 19, c.Y - 19, 38.f, 38.f);
            DrawIcon(g, LG[i], c.X, c.Y, 21, TC(1, a), IS); g.Restore(st);
        }
        { // ряд 5: погода и системная информация (иконка + текст, всё по центру)
            float a = stag(ae, 6); SYSTEMTIME t; GetLocalTime(&t);
            struct Seg { int ic; wchar_t t[24]; float w; Color col; } sg[6]; int ns = 0;
            Color gc(255, 48, 209, 88);
            if (s_wx && v.wx.ok) { sg[ns].ic = WxIcon(v.wx.code); swprintf(sg[ns].t, 24, L"%d°", v.wx.temp); sg[ns].col = TC(2, 1); ns++; }
            sg[ns].ic = -1; swprintf(sg[ns].t, 24, L"CPU %d%%", (int)(v.sys.cpu + .5f)); ns++;
            sg[ns].ic = -1; swprintf(sg[ns].t, 24, L"RAM %d%%", v.sys.ram); ns++;
            if (v.sys.bat >= 0) { sg[ns].ic = v.sys.ac ? IC_BOLT : -1; sg[ns].col = gc; swprintf(sg[ns].t, 24, L"BAT %d%%", v.sys.bat); ns++; }
            sg[ns].ic = -1; swprintf(sg[ns].t, 24, L"%02d:%02d", t.wHour, t.wMinute); ns++;
            float tot = 0; const float gap = 16, iw = 19;
            for (int i = 0; i < ns; i++) { RectF r; g.MeasureString(sg[i].t, -1, fS, PointF(0, 0), sfM, &r); sg[i].w = r.Width + (sg[i].ic >= 0 ? iw : 0); tot += sg[i].w + (i ? gap : 0); }
            float x = px + (pw - tot) / 2, cy = py + (s_launch ? 250.f : 212.f);
            GraphicsState st = g.Save(); g.TranslateTransform(0, (1 - a) * 10);
            for (int i = 0; i < ns; i++) {
                float tx = x;
                if (sg[i].ic >= 0) { DrawIcon(g, sg[i].ic, x + 7.5f, cy, 15, Al(sg[i].col, a * .9f), IS); tx += iw; }
                SolidBrush tb(TC(1, a * .62f)); g.DrawString(sg[i].t, -1, fS, RectF(tx, cy - 10, sg[i].w + 6, 20), sfM, &tb);
                x += sg[i].w + gap;
            }
            g.Restore(st);
        }
    }
    g.ResetClip();

    POINT dst{ g_canvasLeft + sx0, 0 }, src{ sx0, 0 }; SIZE sz{ sw, sh };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hw, NULL, &dst, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
}
