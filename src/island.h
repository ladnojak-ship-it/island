// island.h — логика острова: состояния, наведение, клики, анимации
#pragma once
#include "base.h"
void IslandInit(HWND hw);
int IslandTick();                        // 0 = следующий кадр по vsync, >0 = можно спать столько мс
void IslandMouseDown();
void IslandMouseMove();
void IslandMouseUp();
void IslandWheel(int delta);
void IslandNotify(const std::wstring& text);   // всплывашка-уведомление
void IslandMenuOpen(bool open);                // пока открыто меню — остров не сворачивается
