// sysinfo.h — CPU / RAM / батарея и громкость системы
#pragma once
#include "base.h"
struct Sys { float cpu = 0; int ram = 0, bat = -1; bool ac = false, valid = false; };
extern Sys g_sys;
void SysUpdate();                 // вызывать раз в секунду
bool VolInit();
float VolGet(); bool MuteGet();
void VolSet(float v);             // заодно снимает mute
void MuteSet(bool m);
