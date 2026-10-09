// svgpath.h — минимальный разбор SVG-путей (M L H V C S Z, абсолютные и относительные). Без зависимостей от GDI+.
#pragma once
#include <cstdlib>
#include <cctype>

// Sink должен иметь методы: move(x,y), line(x,y), cubic(x1,y1,x2,y2,x,y), close()
template <class S>
void ParseSvgPath(const char* p, S& out) {
    float cx = 0, cy = 0, sx = 0, sy = 0, px2 = 0, py2 = 0; char cmd = 0; bool prevCubic = false;
    auto skip = [&] { while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t' || *p == '\r') p++; };
    auto num = [&](float& v) -> bool { skip(); char* e; double d = strtod(p, &e); if (e == p) return false; v = (float)d; p = e; return true; };
    for (;;) {
        skip(); if (!*p) break;
        if (isalpha((unsigned char)*p)) cmd = *p++; else if (!cmd) break;
        const bool rel = islower((unsigned char)cmd) != 0; const char C = (char)toupper((unsigned char)cmd);
        float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
        switch (C) {
        case 'Z': out.close(); cx = sx; cy = sy; cmd = 0; prevCubic = false; break;
        case 'M':
            if (!num(a) || !num(b)) return; if (rel) { a += cx; b += cy; }
            cx = sx = a; cy = sy = b; out.move(a, b); cmd = rel ? 'l' : 'L'; prevCubic = false; break;
        case 'L':
            if (!num(a) || !num(b)) return; if (rel) { a += cx; b += cy; }
            out.line(a, b); cx = a; cy = b; prevCubic = false; break;
        case 'H':
            if (!num(a)) return; if (rel) a += cx; out.line(a, cy); cx = a; prevCubic = false; break;
        case 'V':
            if (!num(b)) return; if (rel) b += cy; out.line(cx, b); cy = b; prevCubic = false; break;
        case 'C':
            if (!num(a) || !num(b) || !num(c) || !num(d) || !num(e) || !num(f)) return;
            if (rel) { a += cx; b += cy; c += cx; d += cy; e += cx; f += cy; }
            out.cubic(a, b, c, d, e, f); px2 = c; py2 = d; cx = e; cy = f; prevCubic = true; break;
        case 'S':
            if (!num(c) || !num(d) || !num(e) || !num(f)) return;
            if (rel) { c += cx; d += cy; e += cx; f += cy; }
            a = prevCubic ? 2 * cx - px2 : cx; b = prevCubic ? 2 * cy - py2 : cy;
            out.cubic(a, b, c, d, e, f); px2 = c; py2 = d; cx = e; cy = f; prevCubic = true; break;
        default: return;    // дуги и квадратичные кривые не используются в наших иконках
        }
    }
}
