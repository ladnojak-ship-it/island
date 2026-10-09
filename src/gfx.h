// gfx.h — мелкие помощники рисования (скруглённый прямоугольник, смешение цветов)
#pragma once
#include "gdi.h"
inline void RR(GraphicsPath& p, RectF r, float rad) {
    rad = min(rad, min(r.Width, r.Height) / 2); float d = rad * 2;
    if (d <= 0) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90); p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}
inline Color Mix(Color a, Color b, float t) {      // линейное смешение ARGB
    t = cl(t, 0, 1);
    auto m = [&](int x, int y) { return (BYTE)(x + (y - x) * t + .5f); };
    return Color(m(a.GetA(), b.GetA()), m(a.GetR(), b.GetR()), m(a.GetG(), b.GetG()), m(a.GetB(), b.GetB()));
}
