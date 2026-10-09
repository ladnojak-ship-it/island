// shot.h — скриншот (изображение в буфере обмена) «улетает» в остров
#pragma once
#include "gdi.h"
void ShotInit(HINSTANCE hi);
bool ShotGrab(HWND owner);        // true — обработано (или картинки нет), false — буфер занят, повторить позже
bool ShotStep(bool& impact);      // вызывать каждый кадр; true пока картинка летит; impact=true один раз при «попадании»
bool ShotActive();
void ShotShutdown();
