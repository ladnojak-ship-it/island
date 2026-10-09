// theme.h — темы: Apple и Material You (палитра из обоев, аналог matugen)
#pragma once
#include "gdi.h"
extern float thTgt[6][3];                 // bg, fg, acc, btn, btnFg, chip (целевые цвета)
extern std::atomic<bool> g_themeDirty;    // true -> пересчитать палитру
extern std::atomic<int> g_wpVer;          // растёт при смене обоев
void ThemeInit();
bool ThemeStep(float dt);                 // плавный переход цветов; true пока идёт
Color TC(int idx, float a);               // текущий цвет темы с прозрачностью
Color Al(Color c, float a);
void WallpaperThread();
void ThemePreview(int theme, float out[6][3]);
Color ThemeSeed(float hue);                      // цвет-образец для палитры акцента
float ThemeWpHue();                              // оттенок текущих обоев   // палитра темы без переключения (для окна настроек)
