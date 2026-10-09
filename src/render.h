// render.h
#pragma once
#include "gdi.h"
#include "media.h"
#include "weather.h"
#include "sysinfo.h"

struct View {
    float pw=150,ph=34,fade=0;
    int hud=0;
    std::wstring hudText; float hudPop=1,volDisp=0; bool mute=false;
    Media m; Wx wx; Sys sys; Bitmap* art=nullptr;
    float hov[10]={0};
    float press[8]={0};
    float playAnim=0,eqAmp=0,trackAnim=1,glow=0;
    double now=0;
    float slideY=0;
    float screenshotBump=0;
};
struct Lay{float px,py,pw; RectF prog,vb; PointF prev,play,next,mute,launch[5];};
Lay LayoutFor(float pw);
extern int g_canvasLeft;
bool RenderInit(HWND hw);
bool RenderResize();
extern std::atomic<bool> g_resizeReq;
void RenderFrame(const View& v);
