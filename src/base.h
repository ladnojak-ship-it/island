// base.h — общие подключения и мелкие хелперы (без GDI+)
#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
