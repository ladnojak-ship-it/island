// icons.h — векторные иконки (Material Icons), рисуются GDI+ вместо шрифта Segoe MDL2
#pragma once
#include "gdi.h"
#include "icondata.h"
// style: 0 — как в Material (острые углы), 1 — Material Rounded (лёгкое скругление), 2 — стиль SF/iOS (закруглённые, чуть жирнее)
void DrawIcon(Graphics& g, int id, float cx, float cy, float size, Color c, int style = 0);
