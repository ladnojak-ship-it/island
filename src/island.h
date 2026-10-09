// island.h — логика острова: состояния, наведение, клики, анимации
#pragma once
#include "base.h"
void IslandInit(HWND hw);
int IslandTick();
void IslandMouseDown();
void IslandMouseMove();
void IslandMouseUp();
void IslandWheel(int delta);
void IslandNotify(const std::wstring& text);
void IslandMenuOpen(bool open);

// Screenshot burst animation
void IslandScreenshotBurst();   // вызвать сразу после снимка экрана
