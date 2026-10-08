// anim.h — пружины и easing-функции для анимаций
#pragma once
#include <cmath>

struct Spring {                                // физическая пружина (как в iOS)
    float x = 0, v = 0, t = 0, k = 300, c = 22;
    void snap(float a) { x = t = a; v = 0; }
    void step(float dt) { const int n = 4; float h = dt / n; for (int i = 0; i < n; i++) { v += (k * (t - x) - c * v) * h; x += v * h; } }
    bool moving(float e = .15f) const { return fabsf(t - x) > e || fabsf(v) > e * 10; }
};
inline float ease(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); }
inline float backOut(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; float u = t - 1; return 1 + 2.70158f * u * u * u + 1.70158f * u * u; }
