// fonts.h — шрифты тем: Material You -> Google Sans, Apple -> SF Pro (если установлены / лежат в папке fonts)
#pragma once
#include "gdi.h"
void FontsInit();                                  // подхватывает *.ttf / *.otf / *.ttc из папки fonts рядом с exe
Font* MkFont(int theme, float px, bool bold);      // theme: 0 Apple, 1 Material You; удалять вызывающему
std::wstring FontFamilyName(int theme);            // имя семейства (для GDI-контролов)
