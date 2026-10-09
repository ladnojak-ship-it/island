// settings.h — 30+ настроек Dynamic Island
#pragma once
#include "base.h"

// ── Тема ──────────────────────────────────────────────────────────────────────
extern std::atomic<int> s_theme;      // 0=Apple iOS, 1=Material You
extern std::atomic<int> s_dark;       // 0=светлая, 1=тёмная

// ── Внешний вид острова ───────────────────────────────────────────────────────
extern std::atomic<int> s_scale;      // 80..200 %
extern std::atomic<int> s_opacity;    // 60..100 % (прозрачность фона)
extern std::atomic<int> s_blur;       // 0=нет, 1=лёгкий матовый блюр через MultiLayerEffect
extern std::atomic<int> s_cornerR;   // 0=таблетка (max), 1=средний, 2=прямоугольник
extern std::atomic<int> s_border;     // 0=нет канта, 1=тонкий кант, 2=цветной кант
extern std::atomic<int> s_shadow;     // 0=нет, 1=мягкая, 2=глубокая
extern std::atomic<int> s_glow;       // 0=нет, 1=по акценту при музыке
extern std::atomic<int> s_accentMode; // 0=auto(из обоев/системы), 1=белый, 2=цвет обложки

// ── Поведение ────────────────────────────────────────────────────────────────
extern std::atomic<int> s_autohide;   // 0=всегда виден, 1=авто-скрытие
extern std::atomic<int> s_hideDelay;  // 1..5 секунды до скрытия (× 200мс)
extern std::atomic<int> s_animspeed;  // 60..140 % скорость пружин
extern std::atomic<int> s_lite;       // 0=полный рендер, 1=лёгкий (нет теней/blur)
extern std::atomic<int> s_topmost;    // 0=обычный, 1=поверх всех
extern std::atomic<int> s_clickThru;  // 0=кликабельный, 1=клики сквозь остров

// ── Контент ──────────────────────────────────────────────────────────────────
extern std::atomic<int> s_art;        // 0=нет обложки, 1=есть
extern std::atomic<int> s_wx;         // 0=нет погоды, 1=есть
extern std::atomic<int> s_launch;     // 0=нет быстрых кнопок, 1=есть
extern std::atomic<int> s_hud;        // 0=нет всплывашек громкости/батареи, 1=есть
extern std::atomic<int> s_eq;         // 0=старый паттерн, 1=реальный WASAPI EQ
extern std::atomic<int> s_clock;      // 0=HH:MM, 1=HH:MM:SS, 2=12-часовой
extern std::atomic<int> s_sysBar;     // 0=скрыта строка CPU/RAM/BAT, 1=показана

// ── iOS-тема: детали ─────────────────────────────────────────────────────────
extern std::atomic<int> s_iosStyle;   // 0=стандарт, 1=compact (как в iPhone 16), 2=mini
extern std::atomic<int> s_iosLive;    // 0=нет живых активностей, 1=показывать
extern std::atomic<int> s_iosHaptic;  // (зарезервировано, UI только)
extern std::atomic<int> s_iosTint;    // 0=нет тинта, 1=тинт из обложки

// ── Material You: детали ─────────────────────────────────────────────────────
extern std::atomic<int> s_myRipple;   // 0=нет, 1=Material ripple при клике
extern std::atomic<int> s_myShape;    // 0=Squircle, 1=Rounded Rect, 2=Full pill
extern std::atomic<int> s_myMotion;   // 0=стандарт, 1=Expressive (пружинистее)
extern std::atomic<int> s_myElevation;// 0=flat, 1=tonal, 2=filled elevation

// ── Прочее ───────────────────────────────────────────────────────────────────
extern std::atomic<int> s_startup;    // 0=не авtozапуск, 1=автозапуск с Windows

void LoadSettings();
void SaveSettings();
std::wstring GetCity();
void OpenSettings();
bool SettingsDialogMsg(MSG* m);
