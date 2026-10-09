// audio.h — настоящий спектр: захват того, что играет в системе (WASAPI loopback) -> FFT -> полосы как в cava
#pragma once
#include "base.h"
inline constexpr int NB = 16;           // число полос
void AudioThread();                     // фоновый поток
void AudioBands(float out[NB]);         // сглаженные уровни 0..1 (низкие частоты -> высокие)
bool AudioLive();                       // true, если захват работает
