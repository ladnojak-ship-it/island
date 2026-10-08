// media.h — текущий трек (Spotify, браузер и т.д.) через системные медиа-сессии Windows
#pragma once
#include "gdi.h"
struct Media { std::wstring title, artist; bool has = false, playing = false; double pos = 0, dur = 0; ULONGLONG t = 0; };
void MediaThread();                       // фоновый поток
Media MediaGet();                         // копия состояния
void MediaSend(int cmd, double arg = 0);  // 1 play/pause, 2 назад, 3 вперёд, 4 перемотка (arg = секунды)
bool MediaTakeArt(Bitmap*& art);          // true если обложка сменилась (старую удаляет)
