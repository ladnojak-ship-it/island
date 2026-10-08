// render.h — отрисовка острова (GDI+ -> слой с per-pixel альфой)
#pragma once
#include "gdi.h"
#include "media.h"
#include "weather.h"
#include "sysinfo.h"

// Всё, что нужно нарисовать за кадр. Заполняется в island.cpp, рисуется в render.cpp.
struct View {
    float pw = 150, ph = 34;          // размер пилюли
    float fade = 0;                   // 0 компактный .. 1 раскрытый
    int hud = 0;                      // 0 нет, 1 громкость, 2 батарея, 3 дождь, 4 уведомление
    std::wstring hudText; float hudPop = 1, volDisp = 0; bool mute = false;
    Media m; Wx wx; Sys sys; Bitmap* art = nullptr;
    float hov[10] = { 0 };            // наведение: 0 prev, 1 play, 2 next, 3..7 кнопки запуска, 8 прогресс, 9 громкость
    float press[8] = { 0 };           // «нажатие» кнопок (сжатие)
    float playAnim = 0, eqAmp = 0, trackAnim = 1, glow = 0;
    double now = 0;                   // секунды
};
struct Lay { float px, py, pw; RectF prog, vb; PointF prev, play, next, launch[5]; };
Lay LayoutFor(float pw);              // координаты элементов раскрытого острова (для кликов)
extern int g_canvasLeft;              // экранная X-координата левого края холста
bool RenderInit(HWND hw);
void RenderFrame(const View& v);
