// base.h — общие подключения и мелкие хелперы (без GDI+)
#pragma once
#define NOMINMAX
// WIN32_LEAN_AND_MEAN не используем: он вырезает ole2.h, а GDI+ нужны IStream, PROPID, byte, IUnknown, HDC...
#include <windows.h>
#include <objbase.h>   // CoInitializeEx, COINIT_*
#include <ole2.h>      // IStream, IUnknown (требуются для <gdiplus.h>)
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
using std::min; using std::max;

inline constexpr int CW = 440, CH = 300;      // размер холста острова (логические px)
extern std::atomic<bool> g_run;               // false -> потоки завершаются
extern float g_S;                             // масштаб DPI
inline float cl(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
