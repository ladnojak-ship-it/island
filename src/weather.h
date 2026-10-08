// weather.h — погода и предупреждение о дожде (Open-Meteo, без ключа)
#pragma once
#include "base.h"
struct Wx { bool ok = false; int temp = 0, code = 0, seq = 0; std::wstring text; };   // seq растёт при новом предупреждении
extern std::atomic<bool> g_wxReset;       // true -> обновить сразу (сменили город)
void WeatherThread();
Wx WxGet();
const wchar_t* WxSym(int code);
