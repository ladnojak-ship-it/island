// settings.h — настройки (реестр HKCU\Software\WinIsland) и окно настроек
#pragma once
#include "base.h"
// s_theme: 0 Apple, 1 Material You; s_scale: 80..200 (%); s_round: 0 круглый, 1 мягкий, 2 строгий;
// s_auto: автоскрытие; s_hide: через сколько секунд бездействия прятать; s_top: отступ сверху (px); s_accent: -1 из обоев, иначе оттенок;
// s_sys: показывать CPU/RAM/батарею; s_viz: настоящий визуализатор; s_shot: анимация скриншота
extern std::atomic<int> s_theme, s_dark, s_wx, s_launch, s_art, s_hud, s_lite, s_scale, s_round, s_auto, s_hide, s_top, s_accent, s_sys, s_viz, s_shot;
void LoadSettings();
void SaveSettings();
std::wstring GetCity();
void OpenSettings();
bool SettingsOpen();
bool SettingsDialogMsg(MSG* m);   // true если сообщение съело окно настроек
