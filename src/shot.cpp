#include "shot.h"
#include "settings.h"
#include "render.h"
#include "gfx.h"
#include "anim.h"

static const int FW = 280, FH = 190;                     // максимум размера летящей картинки
static HWND hf = nullptr; static HDC fdc = nullptr; static HBITMAP fbm = nullptr; static void* fbits = nullptr;
static Bitmap* fbmp = nullptr; static Graphics* fg = nullptr;
static Bitmap* thumb = nullptr; static bool active = false; static ULONGLONG t0 = 0, lastStart = 0;
static POINT from{}, to{}; static float tw = 1, th = 1;
static const double DUR = 0.52;

void ShotInit(HINSTANCE hi) {
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hi; wc.lpszClassName = L"WinIslandFly"; RegisterClassW(&wc);
    hf = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"WinIslandFly", L"", WS_POPUP, 0, 0, 1, 1, 0, 0, hi, 0);
    BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), FW, -FH, 1, 32, BI_RGB };
    fbm = CreateDIBSection(0, &bi, DIB_RGB_COLORS, &fbits, 0, 0); fdc = CreateCompatibleDC(0); SelectObject(fdc, fbm);
    fbmp = new Bitmap(FW, FH, FW * 4, PixelFormat32bppPARGB, (BYTE*)fbits); fg = new Graphics(fbmp);
    fg->SetSmoothingMode(SmoothingModeHighQuality); fg->SetInterpolationMode(InterpolationModeHighQualityBicubic); fg->SetPixelOffsetMode(PixelOffsetModeHighQuality);
}
void ShotShutdown() { active = false; delete thumb; thumb = nullptr; delete fg; delete fbmp; fg = nullptr; fbmp = nullptr; if (hf) DestroyWindow(hf); hf = nullptr; }
bool ShotActive() { return active; }

static Bitmap* MakeThumb(BITMAPINFO* bi) {
    BITMAPINFOHEADER& b = bi->bmiHeader;
    if (b.biWidth <= 0 || b.biHeight == 0 || b.biBitCount < 24) return nullptr;
    size_t off = b.biSize + (size_t)b.biClrUsed * sizeof(RGBQUAD);
    if (b.biCompression == BI_BITFIELDS && b.biSize == sizeof(BITMAPINFOHEADER)) off += 12;
    Bitmap src(bi, (BYTE*)bi + off); if (src.GetLastStatus() != Ok) return nullptr;
    float sw = (float)src.GetWidth(), sh = (float)src.GetHeight(); if (sw < 1 || sh < 1) return nullptr;
    float sc = min(1.f, min(260.f / sw, 170.f / sh)); int w = max(24, (int)(sw * sc)), h = max(24, (int)(sh * sc));
    Bitmap* t = new Bitmap(w, h, PixelFormat32bppPARGB); Graphics g(t);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic); g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.DrawImage(&src, Rect(0, 0, w, h), 0, 0, (INT)sw, (INT)sh, UnitPixel);
    return t;
}

bool ShotGrab(HWND owner) {
    if (!s_shot || active) return true;
    if (!IsClipboardFormatAvailable(CF_DIB)) return true;
    if (!OpenClipboard(owner)) return false;
    Bitmap* t = nullptr; HANDLE h = GetClipboardData(CF_DIB);
    if (h) { BITMAPINFO* bi = (BITMAPINFO*)GlobalLock(h); if (bi) { t = MakeThumb(bi); GlobalUnlock(h); } }
    CloseClipboard();
    ULONGLONG now = GetTickCount64();
    if (t && now - lastStart > 700) {
        lastStart = now; delete thumb; thumb = t; tw = (float)t->GetWidth(); th = (float)t->GetHeight();
        POINT c; GetCursorPos(&c); from = c;                                   // летит оттуда, где только что отпустили мышь
        to.x = g_canvasLeft + (LONG)(CW / 2 * g_S); to.y = (LONG)((6 + s_top + 20) * g_S);
        t0 = now; active = true; ShowWindow(hf, SW_SHOWNOACTIVATE);
    } else delete t;
    return true;
}

bool ShotStep(bool& impact) {
    impact = false; if (!active) return false;
    double t = (GetTickCount64() - t0) / (DUR * 1000.0);
    if (t >= 1.0) { active = false; ShowWindow(hf, SW_HIDE); delete thumb; thumb = nullptr; impact = true; return false; }
    float e = (float)(t * t * (2.2 - 1.2 * t));                                // разгоняется к острову
    float s = 1.f - .88f * ease((float)t);
    float w = max(8.f, tw * s), h = max(8.f, th * s);
    float cx = from.x + (to.x - from.x) * e, cy = from.y + (to.y - from.y) * e - sinf(3.14159f * e) * 40.f * g_S;
    int iw = (int)ceilf(w) + 2, ih = (int)ceilf(h) + 2;
    fg->SetCompositingMode(CompositingModeSourceCopy);
    { SolidBrush clr(Color(0, 0, 0, 0)); fg->FillRectangle(&clr, 0, 0, min(FW, iw + 2), min(FH, ih + 2)); }
    fg->SetCompositingMode(CompositingModeSourceOver);
    GraphicsPath p; RR(p, RectF(1, 1, w, h), max(3.f, 10.f * s));
    TextureBrush tb(thumb, WrapModeClamp); tb.ScaleTransform(w / tw, h / th); tb.TranslateTransform(1, 1, MatrixOrderAppend);
    fg->FillPath(&tb, &p);
    { Pen edge(Color(70, 255, 255, 255), 1.f); fg->DrawPath(&edge, &p); }
    POINT dst{ (LONG)(cx - iw / 2.f), (LONG)(cy - ih / 2.f) }, src{ 0, 0 }; SIZE sz{ min(FW, iw), min(FH, ih) };
    BYTE a = (BYTE)(t < .8 ? 255 : 255 - (t - .8) / .2 * 120);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, a, AC_SRC_ALPHA };
    UpdateLayeredWindow(hf, NULL, &dst, &sz, fdc, &src, 0, &bf, ULW_ALPHA);
    return true;
}
