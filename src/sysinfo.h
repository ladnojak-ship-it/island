// sysinfo.h — CPU / RAM / батарея, громкость и WASAPI-захват для EQ
#pragma once
#include "base.h"
struct Sys { float cpu = 0; int ram = 0, bat = -1; bool ac = false, valid = false; };
extern Sys g_sys;
void SysUpdate();
bool VolInit();
float VolGet(); bool MuteGet();
void VolSet(float v);
void MuteSet(bool m);

// WASAPI EQ: 8 частотных полос, обновляются в фоне
static constexpr int EQ_BANDS = 8;
extern float g_eqBands[EQ_BANDS];   // 0..1 амплитуда каждой полосы
void EqCaptureStart();
void EqCaptureStop();
