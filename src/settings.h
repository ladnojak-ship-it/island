// settings.h — настройки (реестр HKCU\Software\WinIsland) и окно настроек
#pragma once
#include "base.h"
extern std::atomic<int> s_theme, s_dark, s_wx, s_launch, s_art, s_hud, s_lite, s_scale;   // s_theme: 0 Apple, 1 Material You; s_scale: 100..150 (%)
void LoadSettings();
void SaveSettings();
std::wstring GetCity();
void OpenSettings();
bool SettingsDialogMsg(MSG* m);   // true если сообщение съело окно настроек
