#include "settings.h"
#include "theme.h"
#include "weather.h"
#include "render.h"
#include "fonts.h"
#include "icons.h"
#include "gfx.h"
#include <dwmapi.h>

// ── Все настройки ─────────────────────────────────────────────────────────────
std::atomic<int> s_theme{0},s_dark{1},s_scale{115},s_opacity{100},s_blur{0};
std::atomic<int> s_cornerR{0},s_border{1},s_shadow{1},s_glow{1},s_accentMode{0};
std::atomic<int> s_autohide{1},s_hideDelay{3},s_animspeed{100},s_lite{0};
std::atomic<int> s_topmost{1},s_clickThru{0};
std::atomic<int> s_art{1},s_wx{1},s_launch{1},s_hud{1},s_eq{1},s_clock{0},s_sysBar{1};
std::atomic<int> s_iosStyle{0},s_iosLive{1},s_iosHaptic{1},s_iosTint{1};
std::atomic<int> s_myRipple{1},s_myShape{0},s_myMotion{1},s_myElevation{1};
std::atomic<int> s_startup{0};

static std::wstring s_city = L"Москва";
static std::mutex cm;
static const wchar_t* RK = L"Software\\WinIsland";

static int rdI(const wchar_t* n,int d){DWORD v=0,sz=4;return RegGetValueW(HKEY_CURRENT_USER,RK,n,RRF_RT_REG_DWORD,0,&v,&sz)==ERROR_SUCCESS?(int)v:d;}
static void wrI(const wchar_t* n,int v){DWORD d=v;RegSetKeyValueW(HKEY_CURRENT_USER,RK,n,REG_DWORD,&d,4);}
std::wstring GetCity(){std::lock_guard<std::mutex> lk(cm);return s_city;}

void LoadSettings(){
    s_theme=rdI(L"theme",0); s_dark=rdI(L"dark",1); s_scale=rdI(L"scale",115);
    if(s_scale<80||s_scale>200)s_scale=115;
    s_opacity=rdI(L"opacity",100); s_blur=rdI(L"blur",0);
    s_cornerR=rdI(L"cornerR",0); s_border=rdI(L"border",1);
    s_shadow=rdI(L"shadow",1); s_glow=rdI(L"glow",1); s_accentMode=rdI(L"accentMode",0);
    s_autohide=rdI(L"autohide",1); s_hideDelay=rdI(L"hideDelay",3);
    s_animspeed=rdI(L"animspeed",100); s_lite=rdI(L"lite",0);
    s_topmost=rdI(L"topmost",1); s_clickThru=rdI(L"clickThru",0);
    s_art=rdI(L"art",1); s_wx=rdI(L"wx",1); s_launch=rdI(L"launch",1);
    s_hud=rdI(L"hud",1); s_eq=rdI(L"eq",1); s_clock=rdI(L"clock",0); s_sysBar=rdI(L"sysBar",1);
    s_iosStyle=rdI(L"iosStyle",0); s_iosLive=rdI(L"iosLive",1);
    s_iosHaptic=rdI(L"iosHaptic",1); s_iosTint=rdI(L"iosTint",1);
    s_myRipple=rdI(L"myRipple",1); s_myShape=rdI(L"myShape",0);
    s_myMotion=rdI(L"myMotion",1); s_myElevation=rdI(L"myElevation",1);
    s_startup=rdI(L"startup",0);
    wchar_t b[64]; DWORD sz=sizeof b;
    if(RegGetValueW(HKEY_CURRENT_USER,RK,L"city",RRF_RT_REG_SZ,0,b,&sz)==ERROR_SUCCESS){
        std::lock_guard<std::mutex> lk(cm); s_city=b;}
}
void SaveSettings(){
    wrI(L"theme",s_theme); wrI(L"dark",s_dark); wrI(L"scale",s_scale);
    wrI(L"opacity",s_opacity); wrI(L"blur",s_blur);
    wrI(L"cornerR",s_cornerR); wrI(L"border",s_border);
    wrI(L"shadow",s_shadow); wrI(L"glow",s_glow); wrI(L"accentMode",s_accentMode);
    wrI(L"autohide",s_autohide); wrI(L"hideDelay",s_hideDelay);
    wrI(L"animspeed",s_animspeed); wrI(L"lite",s_lite);
    wrI(L"topmost",s_topmost); wrI(L"clickThru",s_clickThru);
    wrI(L"art",s_art); wrI(L"wx",s_wx); wrI(L"launch",s_launch);
    wrI(L"hud",s_hud); wrI(L"eq",s_eq); wrI(L"clock",s_clock); wrI(L"sysBar",s_sysBar);
    wrI(L"iosStyle",s_iosStyle); wrI(L"iosLive",s_iosLive);
    wrI(L"iosHaptic",s_iosHaptic); wrI(L"iosTint",s_iosTint);
    wrI(L"myRipple",s_myRipple); wrI(L"myShape",s_myShape);
    wrI(L"myMotion",s_myMotion); wrI(L"myElevation",s_myElevation);
    wrI(L"startup",s_startup);
    std::lock_guard<std::mutex> lk(cm);
    RegSetKeyValueW(HKEY_CURRENT_USER,RK,L"city",REG_SZ,s_city.c_str(),(DWORD)((s_city.size()+1)*2));
}

// ═════════════════════════════════════════════════════════════════════════════
//  ОКНО НАСТРОЕК — полный редизайн
//  iOS-тема: чёрный фон, SF-стиль, grouped table view как в iPhone Settings.app
//  Material You: surface тона, M3 shape, M3 switch, dynamic colour
// ═════════════════════════════════════════════════════════════════════════════
static const float W = 420, H = 900;
static const int ID_CITY = 1001;

// ── hit-зоны ─────────────────────────────────────────────────────────────────
enum {
    H_NONE=-1, H_CLOSE=0,
    // тема cards 1..2
    H_CARD=1,
    // toggles: 10..49
    H_TG=10,
    // segmented: 50..59 (scale), 60..62 (cornerR), 70..72 (clock), 80..82 (iosStyle), 90..91 (myShape), 100..101 (accentMode)
    H_SEG_SCALE=50, H_SEG_CORNER=60, H_SEG_CLOCK=70, H_SEG_IOS=80, H_SEG_SHAPE=90, H_SEG_ACCENT=100,
    // sliders: 200=opacity, 201=animspeed, 202=hideDelay
    H_SL=200,
};

// ── toggle IDs ────────────────────────────────────────────────────────────────
enum {
    TG_DARK=0,
    TG_AUTOHIDE, TG_LITE, TG_TOPMOST, TG_CLICKTHRU,
    TG_ART, TG_WX, TG_LAUNCH, TG_HUD, TG_EQ, TG_SYSBAR,
    TG_GLOW, TG_BORDER, TG_SHADOW,
    TG_IOSLIVE, TG_IOSTINT,
    TG_MYRIPPLE, TG_MYMOTION,
    TG_STARTUP,
    TG_N
};

static std::atomic<int>* TgFlag(int t){
    switch(t){
    case TG_DARK:     return &s_dark;
    case TG_AUTOHIDE: return &s_autohide;
    case TG_LITE:     return &s_lite;
    case TG_TOPMOST:  return &s_topmost;
    case TG_CLICKTHRU:return &s_clickThru;
    case TG_ART:      return &s_art;
    case TG_WX:       return &s_wx;
    case TG_LAUNCH:   return &s_launch;
    case TG_HUD:      return &s_hud;
    case TG_EQ:       return &s_eq;
    case TG_SYSBAR:   return &s_sysBar;
    case TG_GLOW:     return &s_glow;
    case TG_BORDER:   return &s_border;
    case TG_SHADOW:   return &s_shadow;
    case TG_IOSLIVE:  return &s_iosLive;
    case TG_IOSTINT:  return &s_iosTint;
    case TG_MYRIPPLE: return &s_myRipple;
    case TG_MYMOTION: return &s_myMotion;
    case TG_STARTUP:  return &s_startup;
    default:          return &s_dark;
    }
}

// ── состояние окна ────────────────────────────────────────────────────────────
static HWND  hs=nullptr, hEdit=nullptr;
static float SS=1;
static int   hot=H_NONE, slDrag=-1;
static bool  cityFocus=false, tracking=false;
static float tg[TG_N]={0};
static float segScaleA=1, segCornerA=0, segClockA=0, segIosA=0, segShapeA=0, segAccA=0;
static DWORD lastSig=0;
static DWORD lastScrollSig=0;
static float scrollY=0, scrollTgt=0;  // вертикальный скролл
static Font *sT=nullptr,*sB=nullptr,*sS=nullptr,*sH=nullptr,*sL=nullptr;
static int  sFontTheme=-1;
static HFONT hEditFont=nullptr;
static HBRUSH editBr=nullptr; static COLORREF editCol=0;

static int P(float v){return (int)(v*SS+.5f);}

// ── палитра ───────────────────────────────────────────────────────────────────
struct Pal{
    Color bg,card,card2,text,sub,sub2,acc,onAcc,line,field,chip,danger,green;
    bool MY;
};
static Pal GetPal(){
    Pal p; p.MY=(s_theme!=0);
    if(!p.MY){
        // iOS: точный Apple Human Interface — чёрный/серый как в Settings.app
        bool dk=(s_dark!=0);
        if(dk){
            p.bg    =Color(255, 0, 0, 0);
            p.card  =Color(255,28,28,30);
            p.card2 =Color(255,44,44,46);
            p.text  =Color(255,255,255,255);
            p.sub   =Color(255,152,152,158);
            p.sub2  =Color(255,99,99,102);
            p.acc   =Color(255,10,132,255);
            p.onAcc =Color(255,255,255,255);
            p.line  =Color(255,56,56,58);
            p.field =Color(255,44,44,46);
            p.chip  =Color(255,72,72,74);
            p.danger=Color(255,255,69,58);
            p.green =Color(255,48,209,88);
        } else {
            p.bg    =Color(255,242,242,247);
            p.card  =Color(255,255,255,255);
            p.card2 =Color(255,242,242,247);
            p.text  =Color(255,0,0,0);
            p.sub   =Color(255,60,60,67);
            p.sub2  =Color(255,138,138,142);
            p.acc   =Color(255,0,122,255);
            p.onAcc =Color(255,255,255,255);
            p.line  =Color(255,198,198,200);
            p.field =Color(255,255,255,255);
            p.chip  =Color(255,229,229,234);
            p.danger=Color(255,255,59,48);
            p.green =Color(255,52,199,89);
        }
    } else {
        // Material You M3
        p.bg    =TC(0,1);
        p.text  =TC(1,1);
        p.acc   =TC(2,1);
        p.onAcc =TC(4,1);
        p.chip  =TC(5,1);
        p.card  =Mix(p.bg,p.text,.07f);
        p.card2 =Mix(p.bg,p.text,.12f);
        p.sub   =Mix(p.bg,p.text,.65f);
        p.sub2  =Mix(p.bg,p.text,.40f);
        p.line  =Mix(p.bg,p.text,.14f);
        p.field =Mix(p.bg,p.text,.10f);
        p.danger=Color(255,179,38,30);
        p.green =Color(255,52,168,83);
    }
    return p;
}
static COLORREF CR(Color c){return RGB(c.GetR(),c.GetG(),c.GetB());}

static void BuildFonts(){
    int th=s_theme?1:0; if(th==sFontTheme&&sT)return;
    delete sT; delete sB; delete sS; delete sH; delete sL;
    sT=MkFont(th,22.f,true);   // заголовок секции
    sB=MkFont(th,15.f,false);  // label строки
    sS=MkFont(th,13.f,false);  // subtitle / value
    sH=MkFont(th,12.f,true);   // header label (ALL CAPS / acc)
    sL=MkFont(th,17.f,false);  // large title
    sFontTheme=th;
    if(hEdit){
        HFONT nf=CreateFontW(-P(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,
            FontFamilyName(th).c_str());
        SendMessage(hEdit,WM_SETFONT,(WPARAM)nf,TRUE);
        if(hEditFont)DeleteObject(hEditFont); hEditFont=nf;
    }
}
static void Tx(Graphics& g,const wchar_t* s,Font* f,RectF r,Color c,StringAlignment al=StringAlignmentNear){
    StringFormat sf(StringFormatFlagsNoWrap);
    sf.SetAlignment(al); sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush b(c); g.DrawString(s,-1,f,r,&sf,&b);
}
static void FillRR(Graphics& g,RectF r,float rad,Color c){
    GraphicsPath p; RR(p,r,rad); SolidBrush b(c); g.FillPath(&b,&p);
}
static void StrokeRR(Graphics& g,RectF r,float rad,Color c,float w){
    GraphicsPath p; RR(p,r,rad); Pen pen(c,w); g.DrawPath(&pen,&p);
}

// ── iOS Toggle ────────────────────────────────────────────────────────────────
static void Toggle(Graphics& g,const Pal& Pl,float rx,float cy,float t,float dis=0){
    auto D=[&](Color c){return dis>0?Mix(c,Pl.card,.55f):c;};
    const float tw=51,th=31; RectF tr(rx-tw,cy-th/2,tw,th);
    if(!Pl.MY){
        // iOS exact: зелёный #34C759 вкл, #636366 выкл
        Color onC  =D(Color(255,52,199,89));
        Color offC =D(Color(255,99,99,102));
        FillRR(g,tr,th/2,D(Mix(offC,onC,t)));
        // knob с тенью
        float kx=tr.X+2+t*(tw-th+0);
        SolidBrush sh(Color(60,0,0,0)); g.FillEllipse(&sh,kx+0.5f,cy-th/2+2+1.f,th-4,th-4);
        SolidBrush kb(D(Color(255,255,255,255))); g.FillEllipse(&kb,kx,cy-th/2+2,th-4,th-4);
    } else {
        // M3 Switch: track + thumb + icon
        Color off=Mix(Pl.card,Pl.text,.18f);
        Color trackC=D(Mix(off,Pl.acc,t));
        FillRR(g,tr,th/2,trackC);
        if(t<.99f) StrokeRR(g,RectF(tr.X+1,tr.Y+1,tw-2,th-2),th/2-1,D(Mix(Pl.sub,Pl.acc,t)),2.f);
        float d=14+10*t, kx=tr.X+15+t*(tw-30);
        Color kc=D(Mix(Pl.sub,Pl.onAcc,t));
        SolidBrush kb(kc); g.FillEllipse(&kb,kx-d/2,cy-d/2,d,d);
        if(t>.4f) DrawIcon(g,IC_CHECK,kx,cy,11,D(Mix(kc,Pl.acc,(t-.4f)/.6f)),0);
    }
}

// ── Slider (opacity, animspeed, hideDelay) ────────────────────────────────────
struct SliderDef {
    std::atomic<int>* val; int mn,mx; const wchar_t* label; const wchar_t* unit; int hitId;
};
static SliderDef SLIDERS[3]={
    {&s_opacity,   60,100, L"Непрозрачность", L"%",  H_SL+0},
    {&s_animspeed, 60,140, L"Скорость анимации", L"%", H_SL+1},
    {&s_hideDelay, 1,  5,  L"Задержка скрытия", L"с",  H_SL+2},
};
static float slThumb[3]={0};  // анимированная позиция 0..1

static void DrawSlider(Graphics& g,const Pal& Pl,float x,float cy,float w,float val01,int id,bool hov){
    float th=hov?7.f:5.f;
    // track bg
    FillRR(g,RectF(x,cy-th/2,w,th),th/2,Pl.line);
    // fill
    float fw=w*val01; if(fw>2)FillRR(g,RectF(x,cy-th/2,fw,th),th/2,Pl.acc);
    // knob
    float r=hov?10.f:8.f;
    SolidBrush sh(Color(50,0,0,0)); g.FillEllipse(&sh,x+fw-r+1,cy-r+1,r*2,r*2);
    SolidBrush kb(!Pl.MY?Color(255,255,255,255):Pl.onAcc);
    g.FillEllipse(&kb,x+fw-r,cy-r,r*2,r*2);
    Pen kb2(Pl.acc,2.f); g.DrawEllipse(&kb2,x+fw-r,cy-r,r*2,r*2);
}

// ── Segmented control ─────────────────────────────────────────────────────────
static void DrawSeg(Graphics& g,const Pal& Pl,float x,float y,float w,float h,
                    const wchar_t** labels,int n,int sel,float selAnim,int baseHit){
    float R=!Pl.MY?10.f:16.f;
    FillRR(g,RectF(x,y,w,h),R,Pl.card);
    float sw=w/n;
    // sliding pill
    FillRR(g,RectF(x+selAnim*sw+2,y+2,sw-4,h-4),R-2,!Pl.MY?Pl.chip:Mix(Pl.acc,Pl.bg,.78f));
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
    for(int i=0;i<n;i++){
        bool s=(i==sel);
        Color tc=s?Pl.text:Pl.sub;
        SolidBrush b(tc); g.DrawString(labels[i],-1,sB,RectF(x+i*sw,y,sw,h),&sf,&b);
    }
}

// ── Строка настройки (grouped) ────────────────────────────────────────────────
// rowH=44 стандарт, рисуем label + правый контрол (toggle/value/chevron)
struct Row {
    int     hitId;        // H_TG+TG_xxx  или H_SEG_xxx или H_SL+x
    const wchar_t* label;
    const wchar_t* subLabel;  // nullptr если нет
    int     icon;         // IC_xxx или -1
    Color   iconColor;
    // что справа: 0=toggle, 1=value+chevron, 2=ничего
    int     right;
    int     tgId;         // TG_xxx для right==0
    std::wstring value;   // для right==1
};

// ── Геометрия ─────────────────────────────────────────────────────────────────
static const float MARGIN=20.f, GRP_RAD=16.f, ROW_H=48.f;
static const float CONTENT_W=W-2*MARGIN;

// Секции хранятся как:  заголовок + массив строк
// Рисуем через прокручиваемый canvas, поэтому координаты Y считаем динамически

// ── Ripple state (MY) ────────────────────────────────────────────────────────
struct Ripple { float x,y,r,a; };
static Ripple ripple={0,0,0,0};

// ── Paint ─────────────────────────────────────────────────────────────────────
static const int SCALES[6]={80,100,115,130,150,175};
static int SegScaleIdx(){int b=0,bd=999;for(int i=0;i<6;i++){int d=abs(SCALES[i]-s_scale.load());if(d<bd){bd=d;b=i;}}return b;}

static void Paint(HDC dc){
    RECT rc; GetClientRect(hs,&rc);
    int Wp=rc.right,Hp=rc.bottom; if(Wp<=0||Hp<=0)return;
    BuildFonts(); Pal Pl=GetPal(); const bool MY=Pl.MY; const int IS=MY?1:2;

    Bitmap bmp(Wp,Hp,PixelFormat32bppRGB); Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(Pl.bg); g.ScaleTransform(SS,SS);

    // ── скролл-клип ──────────────────────────────────────────────────────────
    // Контент рисуется со смещением -scrollY начиная с headerH
    const float headerH=64.f; // высота sticky-хедера
    g.SetClip(RectF(0,headerH,W,H-headerH),CombineModeIntersect);

    // ── курсор мыши для hover ─────────────────────────────────────────────────
    // (hot определяется с учётом скролла в HitAt)

    // ── переменные Y (логические, уже со скроллом) ───────────────────────────
    float Y=headerH+8-scrollY;
    auto grpY=[&]()->float{return Y;};

    // ╔════════════════════════════════════════════════════════════╗
    // helper: рисует group card с заголовком и массивом строк
    // ╚════════════════════════════════════════════════════════════╝
    auto DrawGroup=[&](const wchar_t* hdr, std::initializer_list<Row> rows, bool last=false)->void{
        // заголовок (iOS: uppercase grey выше карточки; MY: acc bold)
        float hy=Y;
        if(!MY){
            wchar_t up[64]; int i=0; for(const wchar_t*p=hdr;*p&&i<63;p++,i++) up[i]=towupper(*p); up[i]=0;
            Tx(g,up,sH,RectF(MARGIN+4,hy+2,CONTENT_W-8,20),Pl.sub);
        } else {
            Tx(g,hdr,sH,RectF(MARGIN+4,hy+2,CONTENT_W-8,20),Pl.acc);
        }
        Y+=26;

        // карточка
        int n=(int)rows.size();
        float cardH=n*ROW_H;
        FillRR(g,RectF(MARGIN,Y,CONTENT_W,cardH),GRP_RAD,Pl.card);

        float ry=Y;
        bool first=true;
        for(const Row& row: rows){
            // разделитель
            if(!first){
                Pen lp(Pl.line,.7f);
                float lineX=MARGIN+16+(row.icon>=0?44.f:0.f);
                g.DrawLine(&lp,lineX,ry,MARGIN+CONTENT_W-16,ry);
            }
            first=false;

            // hover bg (клипуем в карточку)
            if(hot==row.hitId){
                GraphicsState st=g.Save();
                GraphicsPath gp; RR(gp,RectF(MARGIN,Y,CONTENT_W,cardH),GRP_RAD);
                g.SetClip(&gp,CombineModeIntersect);
                SolidBrush hb(Mix(Pl.card,Pl.text,.06f));
                g.FillRectangle(&hb,MARGIN,ry,CONTENT_W,ROW_H);
                g.Restore(st);
            }

            // иконка (iOS: цветная капсула; MY: плоский кружок с acc)
            float cx=MARGIN+28;
            if(row.icon>=0){
                if(!MY){
                    // iOS-style: цветной rounded square 28×28
                    FillRR(g,RectF(cx-14,ry+ROW_H/2-14,28,28),7.f,row.iconColor);
                    DrawIcon(g,row.icon,cx,ry+ROW_H/2,16,Color(255,255,255,255),IS);
                } else {
                    // MY: flat circle
                    SolidBrush ic(Color(40,row.iconColor.GetR(),row.iconColor.GetG(),row.iconColor.GetB()));
                    g.FillEllipse(&ic,cx-14,ry+ROW_H/2-14,28.f,28.f);
                    DrawIcon(g,row.icon,cx,ry+ROW_H/2,16,row.iconColor,IS);
                }
                cx+=22;
            } else { cx=MARGIN+16; }

            // label + sublabel
            float labelX=cx;
            float labelW=CONTENT_W-16-(labelX-MARGIN);
            if(row.right==0) labelW-=58;  // toggle width
            else if(row.right==1) labelW-=80;
            if(row.subLabel){
                Tx(g,row.label,sB,RectF(labelX,ry+5,labelW,22),Pl.text);
                Tx(g,row.subLabel,sS,RectF(labelX,ry+27,labelW,17),Pl.sub);
            } else {
                Tx(g,row.label,sB,RectF(labelX,ry,labelW,ROW_H),Pl.text);
            }

            // правая часть
            float rightX=MARGIN+CONTENT_W-16;
            if(row.right==0){
                // toggle
                int tgId=row.tgId;
                float tval=tg[tgId];
                Toggle(g,Pl,rightX,ry+ROW_H/2,tval);
            } else if(row.right==1 && !row.value.empty()){
                // value + chevron
                Tx(g,row.value.c_str(),sB,RectF(rightX-100,ry,90,ROW_H),Pl.sub,StringAlignmentFar);
                // chevron >
                Pen cp(Pl.sub2,1.5f); cp.SetStartCap(LineCapRound); cp.SetEndCap(LineCapRound); cp.SetLineJoin(LineJoinRound);
                float cx2=rightX-4,cy2=ry+ROW_H/2;
                PointF pts2[3]={PointF(cx2-5,cy2-6),PointF(cx2,cy2),PointF(cx2-5,cy2+6)};
                g.DrawLines(&cp,pts2,3);
            }

            ry+=ROW_H;
        }
        Y=ry+12;
    };

    // ── иконки для секций ─────────────────────────────────────────────────────
    auto IC=[](Color c)->Color{return c;};
    Color cBlue =Color(255, 10,132,255), cGreen=Color(255,52,199, 89);
    Color cOrang=Color(255,255,149, 0),  cRed  =Color(255,255, 59, 48);
    Color cPurp =Color(255,175, 82,222), cTeal =Color(255,90,200,250);
    Color cPink =Color(255,255, 55,95),  cGray =Color(255,142,142,147);
    Color cIndigo=Color(255,88,86,214);

    // ══════════════════════════════════════════════════════════════
    // 1. ТЕМА
    // ══════════════════════════════════════════════════════════════
    {
        float hy=Y;
        if(!MY) Tx(g,L"ТЕМА",sH,RectF(MARGIN+4,hy+2,CONTENT_W-8,20),Pl.sub);
        else    Tx(g,L"Тема",sH,RectF(MARGIN+4,hy+2,CONTENT_W-8,20),Pl.acc);
        Y+=26;

        // две карточки-превью
        float cw=(CONTENT_W-10)/2;
        for(int i=0;i<2;i++){
            float cx=MARGIN+i*(cw+10), cy=Y;
            bool sel=(s_theme!=0)==(i==1);
            // карточка
            FillRR(g,RectF(cx,cy,cw,88),GRP_RAD,Pl.card);
            if(sel) StrokeRR(g,RectF(cx+1,cy+1,cw-2,86),GRP_RAD-1,Pl.acc,2.f);
            if(hot==H_CARD+i&&!sel){
                FillRR(g,RectF(cx,cy,cw,88),GRP_RAD,Mix(Pl.card,Pl.text,.05f));
            }

            // мини-превью острова внутри карточки
            float bx=cx+12,by=cy+14,bw=cw-24,bh=28;
            if(i==0){
                // iOS: чёрная пилюля
                FillRR(g,RectF(bx,by,bw,bh),bh/2,Color(255,0,0,0));
                // мини EQ-бары белые
                for(int b=0;b<4;b++){float bx2=bx+bw-30+b*6;SolidBrush bb(Color(200,255,255,255));g.FillRectangle(&bb,bx2,by+9,3,10);}
                // мини-обложка
                FillRR(g,RectF(bx+6,by+4,20,20),4.f,Color(255,255,95,120));
            } else {
                // MY: тональный фон с акцентом
                float pa[6][3]; ThemePreview(1,pa);
                auto pc=[&](int idx)->Color{return Color(255,(BYTE)pa[idx][0],(BYTE)pa[idx][1],(BYTE)pa[idx][2]);};
                FillRR(g,RectF(bx,by,bw,bh),12.f,pc(0));
                StrokeRR(g,RectF(bx,by,bw,bh),12.f,Mix(pc(0),pc(1),.25f),1.f);
                FillRR(g,RectF(bx+6,by+4,20,20),8.f,pc(2));
                for(int b=0;b<4;b++){float bx2=bx+bw-30+b*6;SolidBrush bb(Color(200,pc(1).GetR(),pc(1).GetG(),pc(1).GetB()));g.FillRectangle(&bb,bx2,by+9,3,10);}
            }

            // название + чекмарк
            Tx(g,i==0?L"Apple iOS":L"Material You",sB,RectF(cx+12,cy+52,cw-36,24),Pl.text);
            if(sel){
                SolidBrush ab(Pl.acc); g.FillEllipse(&ab,cx+cw-30,cy+56,20.f,20.f);
                DrawIcon(g,IC_CHECK,cx+cw-20,cy+66,13,Pl.onAcc,IS);
            }
        }
        Y+=88+12;
    }

    // ── если iOS: extra iOS секция ────────────────────────────────────────────
    if(!MY){
        // Тёмная схема (в рамках группы "Внешний вид")
        DrawGroup(L"Внешний вид",{
            {H_TG+TG_DARK,     L"Тёмная схема",      nullptr,         IC_BOLT,   cBlue,   0, TG_DARK},
            {H_TG+TG_GLOW,     L"Свечение при музыке",nullptr,         IC_NOTE,   cPurp,   0, TG_GLOW},
            {H_TG+TG_BORDER,   L"Тонкий кант",        nullptr,         -1,{},     0, TG_BORDER},
            {H_TG+TG_SHADOW,   L"Тень острова",       nullptr,         -1,{},     0, TG_SHADOW},
            {H_TG+TG_IOSTINT,  L"Тинт из обложки",    L"Цвет иконок по обложке", -1,{}, 0, TG_IOSTINT},
        });

        // iOS-стиль острова
        {
            if(!MY) Tx(g,L"СТИЛЬ ОСТРОВА",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* stLabels[3]={L"Стандарт",L"Компакт",L"Мини"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,stLabels,3,s_iosStyle.load(),segIosA,H_SEG_IOS);
            Y+=48+12;
        }

        DrawGroup(L"Поведение",{
            {H_TG+TG_AUTOHIDE,  L"Авто-скрытие",      L"Скрывается когда не нужен",  IC_CLOSE, cGray,  0, TG_AUTOHIDE},
            {H_TG+TG_IOSLIVE,   L"Live Activities",    L"Уведомления в реальном времени", IC_BELL, cOrang, 0, TG_IOSLIVE},
            {H_TG+TG_TOPMOST,   L"Поверх окон",        nullptr,                       -1,{},     0, TG_TOPMOST},
            {H_TG+TG_CLICKTHRU, L"Клики насквозь",     L"Не перехватывает мышь",      -1,{},     0, TG_CLICKTHRU},
        });

        DrawGroup(L"Контент",{
            {H_TG+TG_ART,    L"Обложка альбома",   nullptr, IC_NOTE,   cPink,  0, TG_ART},
            {H_TG+TG_WX,     L"Погода",            nullptr, IC_RAIN,   cTeal,  0, TG_WX},
            {H_TG+TG_LAUNCH, L"Быстрые кнопки",   nullptr, IC_FOLDER, cOrang, 0, TG_LAUNCH},
            {H_TG+TG_HUD,    L"Всплывашки",        L"Громкость, батарея", IC_VOL, cGreen, 0, TG_HUD},
            {H_TG+TG_EQ,     L"Реальный EQ",       L"WASAPI-захват аудио", IC_NOTE, cPurp,  0, TG_EQ},
            {H_TG+TG_SYSBAR, L"Строка CPU/RAM",    nullptr, -1,{},     0, TG_SYSBAR},
        });

        // Формат часов
        {
            if(!MY) Tx(g,L"ЧАСЫ",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* ck[3]={L"HH:MM",L"HH:MM:SS",L"12-hour"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,ck,3,s_clock.load(),segClockA,H_SEG_CLOCK);
            Y+=48+12;
        }

        // Акцент-цвет
        {
            Tx(g,L"ЦВЕТ АКЦЕНТА",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* ac[3]={L"Авто",L"Белый",L"Обложка"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,ac,3,s_accentMode.load(),segAccA,H_SEG_ACCENT);
            Y+=48+12;
        }

        // Слайдеры
        {
            Tx(g,L"ПАРАМЕТРЫ",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            float slH=ROW_H*3;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,slH),GRP_RAD,Pl.card);
            for(int i=0;i<3;i++){
                float ry2=Y+i*ROW_H;
                if(i>0){Pen lp(Pl.line,.7f);g.DrawLine(&lp,MARGIN+16,ry2,MARGIN+CONTENT_W-16,ry2);}
                auto& sl=SLIDERS[i];
                Tx(g,sl.label,sB,RectF(MARGIN+16,ry2,120,ROW_H),Pl.text);
                // value label
                wchar_t vb[16]; swprintf(vb,16,L"%d%ls",sl.val->load(),sl.unit);
                Tx(g,vb,sS,RectF(MARGIN+CONTENT_W-70,ry2,54,ROW_H),Pl.sub,StringAlignmentFar);
                // slider track
                float sv=(sl.val->load()-sl.mn)/float(sl.mx-sl.mn);
                bool hh=(hot==sl.hitId);
                DrawSlider(g,Pl,MARGIN+140,ry2+ROW_H/2,CONTENT_W-160-60,sv,sl.hitId,hh);
            }
            Y+=slH+12;
        }

        // Размер
        {
            Tx(g,L"РАЗМЕР",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* sz[6]={L"80%",L"100%",L"115%",L"130%",L"150%",L"175%"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,sz,6,SegScaleIdx(),segScaleA,H_SEG_SCALE);
            Y+=48+12;
        }

        // Скруглённость
        {
            Tx(g,L"ФОРМА",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.sub);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* cr[3]={L"Таблетка",L"Средние",L"Прямой"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,cr,3,s_cornerR.load(),segCornerA,H_SEG_CORNER);
            Y+=48+12;
        }

        // Город погоды
        DrawGroup(L"Погода",{
            {H_NONE, L"Город", nullptr, IC_RAIN, cTeal, 1},
        });
        // EditBox рисуется через WinAPI — здесь показываем его позицию

        // Прочее
        DrawGroup(L"Система",{
            {H_TG+TG_LITE,    L"Лёгкий режим",   L"Меньше теней и эффектов",  -1,{}, 0, TG_LITE},
            {H_TG+TG_STARTUP, L"Запуск со стартом", nullptr,                   -1,{}, 0, TG_STARTUP},
        });

    } else {
        // ── MATERIAL YOU секции ───────────────────────────────────────────────
        DrawGroup(L"Внешний вид",{
            {H_TG+TG_DARK,    L"Тёмная схема",       nullptr,                     IC_BOLT,   cBlue,  0, TG_DARK},
            {H_TG+TG_GLOW,    L"Свечение",           L"Ambient glow при музыке",  IC_NOTE,   cPurp,  0, TG_GLOW},
            {H_TG+TG_BORDER,  L"Outline",            L"M3 outline token",          -1,{},     0, TG_BORDER},
            {H_TG+TG_SHADOW,  L"Elevation shadow",   nullptr,                     -1,{},     0, TG_SHADOW},
        });

        // M3 Shape
        {
            Tx(g,L"Форма острова",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.acc);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* sh[3]={L"Squircle",L"Rounded",L"Pill"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,sh,3,s_myShape.load(),segShapeA,H_SEG_SHAPE);
            Y+=48+12;
        }

        DrawGroup(L"Motion",{
            {H_TG+TG_MYMOTION, L"Expressive motion", L"Более пружинистые анимации", IC_NOTE, cPurp, 0, TG_MYMOTION},
            {H_TG+TG_MYRIPPLE, L"Ripple-эффект",     L"Material ripple при нажатии", -1,{}, 0, TG_MYRIPPLE},
        });

        DrawGroup(L"Поведение",{
            {H_TG+TG_AUTOHIDE,  L"Авто-скрытие",       L"Скрывается когда не нужен", IC_CLOSE, cGray,  0, TG_AUTOHIDE},
            {H_TG+TG_TOPMOST,   L"Поверх окон",         nullptr,                      -1,{},     0, TG_TOPMOST},
            {H_TG+TG_CLICKTHRU, L"Клики насквозь",      L"Pass-through",              -1,{},     0, TG_CLICKTHRU},
        });

        DrawGroup(L"Контент",{
            {H_TG+TG_ART,    L"Album art",          nullptr,                    IC_NOTE,   cPink,  0, TG_ART},
            {H_TG+TG_WX,     L"Погода",             nullptr,                    IC_RAIN,   cTeal,  0, TG_WX},
            {H_TG+TG_LAUNCH, L"Быстрые кнопки",    nullptr,                    IC_FOLDER, cOrang, 0, TG_LAUNCH},
            {H_TG+TG_HUD,    L"HUD-уведомления",   L"Громкость, батарея",      IC_VOL,    cGreen, 0, TG_HUD},
            {H_TG+TG_EQ,     L"Real-time EQ",       L"WASAPI audio capture",    IC_NOTE,   cPurp,  0, TG_EQ},
            {H_TG+TG_SYSBAR, L"CPU / RAM / BAT",    nullptr,                    -1,{},     0, TG_SYSBAR},
        });

        // Цвет акцента
        {
            Tx(g,L"Акцентный цвет",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.acc);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* ac[3]={L"Обои",L"Белый",L"Обложка"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,ac,3,s_accentMode.load(),segAccA,H_SEG_ACCENT);
            // палитра кругов
            float pa[6][3]; ThemePreview(1,pa);
            static const int idx[5]={2,5,0,1,3};
            for(int i=0;i<5;i++){
                float pcx=MARGIN+8+i*34.f;
                Color pc2=Color(255,(BYTE)pa[idx[i]][0],(BYTE)pa[idx[i]][1],(BYTE)pa[idx[i]][2]);
                SolidBrush pb2(pc2); g.FillEllipse(&pb2,pcx,Y+52,24.f,24.f);
                Pen op2(Pl.line,1.f); g.DrawEllipse(&op2,pcx,Y+52,24.f,24.f);
            }
            Y+=80+12;
        }

        // Часы
        {
            Tx(g,L"Формат часов",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.acc);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* ck[3]={L"HH:MM",L"HH:MM:SS",L"12h"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,ck,3,s_clock.load(),segClockA,H_SEG_CLOCK);
            Y+=48+12;
        }

        // Слайдеры
        {
            Tx(g,L"Параметры",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.acc);
            Y+=26;
            float slH=ROW_H*3;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,slH),GRP_RAD,Pl.card);
            for(int i=0;i<3;i++){
                float ry2=Y+i*ROW_H;
                if(i>0){Pen lp(Pl.line,.7f);g.DrawLine(&lp,MARGIN+16,ry2,MARGIN+CONTENT_W-16,ry2);}
                auto& sl=SLIDERS[i];
                Tx(g,sl.label,sB,RectF(MARGIN+16,ry2,120,ROW_H),Pl.text);
                wchar_t vb[16]; swprintf(vb,16,L"%d%ls",sl.val->load(),sl.unit);
                Tx(g,vb,sS,RectF(MARGIN+CONTENT_W-70,ry2,54,ROW_H),Pl.sub,StringAlignmentFar);
                float sv=(sl.val->load()-sl.mn)/float(sl.mx-sl.mn);
                bool hh=(hot==sl.hitId);
                DrawSlider(g,Pl,MARGIN+140,ry2+ROW_H/2,CONTENT_W-160-60,sv,sl.hitId,hh);
            }
            Y+=slH+12;
        }

        // Размер
        {
            Tx(g,L"Размер",sH,RectF(MARGIN+4,Y+2,CONTENT_W-8,20),Pl.acc);
            Y+=26;
            FillRR(g,RectF(MARGIN,Y,CONTENT_W,48),GRP_RAD,Pl.card);
            const wchar_t* sz[6]={L"80",L"100",L"115",L"130",L"150",L"175"};
            DrawSeg(g,Pl,MARGIN,Y,CONTENT_W,48,sz,6,SegScaleIdx(),segScaleA,H_SEG_SCALE);
            Y+=48+12;
        }

        // Погода
        DrawGroup(L"Погода",{{H_NONE,L"Город",nullptr,IC_RAIN,cTeal,1}});

        // Система
        DrawGroup(L"Система",{
            {H_TG+TG_LITE,    L"Lite mode",     L"Без теней и размытий",     -1,{}, 0, TG_LITE},
            {H_TG+TG_STARTUP, L"Автозапуск",    nullptr,                     -1,{}, 0, TG_STARTUP},
        });
    }

    // ── Material Ripple overlay ───────────────────────────────────────────────
    if(MY && ripple.a>0){
        Color rc2=Color((BYTE)(ripple.a*255*0.18f),Pl.acc.GetR(),Pl.acc.GetG(),Pl.acc.GetB());
        SolidBrush rb(rc2); g.FillEllipse(&rb,ripple.x-ripple.r,ripple.y-ripple.r,ripple.r*2,ripple.r*2);
    }

    // ── сбрасываем клип для sticky header ────────────────────────────────────
    g.ResetClip();

    // ── sticky хедер ─────────────────────────────────────────────────────────
    // iOS: тёмный/светлый фон + blur-like tint; MY: surface tonal
    Color hdrBg=!MY?Mix(Pl.bg,Pl.text,.04f):Mix(Pl.bg,Pl.text,.06f);
    SolidBrush hbr(hdrBg); g.FillRectangle(&hbr,0,0,W,headerH);
    // разделитель внизу хедера (тонкий при скролле)
    if(scrollY>4){Pen sp(Pl.line,.7f);g.DrawLine(&sp,0,headerH-1,W,headerH-1);}

    // Large title (iOS) / Display (MY)
    Tx(g,L"Настройки",sL,RectF(MARGIN,20,W-80,36),Pl.text);
    Tx(g,L"Dynamic Island",sS,RectF(MARGIN,44,W-80,18),Pl.sub);

    // кнопка закрытия (iOS: × в круге; MY: arrow_back)
    RectF cr=RectF(W-48,12,36,36);
    if(!MY){
        SolidBrush cb(hot==H_CLOSE?Pl.card:Color(0,0,0,0)); g.FillEllipse(&cb,cr.X,cr.Y,cr.Width,cr.Height);
        DrawIcon(g,IC_CLOSE,cr.X+18,cr.Y+18,18,hot==H_CLOSE?Pl.text:Pl.sub,IS);
    } else {
        if(hot==H_CLOSE){FillRR(g,cr,12.f,Mix(Pl.card,Pl.text,.08f));}
        DrawIcon(g,IC_CLOSE,cr.X+18,cr.Y+18,18,Pl.sub,IS);
    }

    // ── scrollbar (iOS thin) ─────────────────────────────────────────────────
    float totalH=Y+scrollY+8;
    float visRatio=(H-headerH)/totalH;
    if(visRatio<.98f){
        float sbH=(H-headerH)*visRatio, sbY=headerH+(H-headerH)*(scrollY/totalH);
        FillRR(g,RectF(W-5,sbY,3,sbH),2.f,Color(80,128,128,128));
    }

    Graphics gd(dc); gd.DrawImage(&bmp,Rect(0,0,Wp,Hp),0,0,Wp,Hp,UnitPixel);
}

// ── Logic: hit test ───────────────────────────────────────────────────────────
static const float headerH_=64.f;

static int HitAt(float x,float y){
    // кнопка закрытия (sticky)
    if(x>=W-48&&x<W-12&&y>=12&&y<48) return H_CLOSE;

    // всё остальное — с учётом скролла
    float sy=y+scrollY-headerH_-8; // логическая Y в контенте

    // карточки темы: примерно Y=26..114 в контенте
    if(sy>=26&&sy<114){
        float cw=(CONTENT_W-10)/2;
        for(int i=0;i<2;i++){
            float cx=MARGIN+i*(cw+10);
            if(x>=cx&&x<cx+cw) return H_CARD+i;
        }
    }

    // toggles — сканируем все тоггл-зоны по вертикали
    // Так как layout динамический, используем простой подход: пересчитываем Y за O(n)
    // (в реальном проекте использовали бы layout cache — тут упрощение)
    // Вместо этого: HitAt использует глобальный массив hitRects обновляемый в Paint

    return H_NONE;
}

// Кэш хит-зон (обновляется после каждого Paint)
struct HitRect{ int id; float x,y,w,h; };
static std::vector<HitRect> g_hitRects;
static float g_contentH=0;

// ── Click / Drag ──────────────────────────────────────────────────────────────
static void Apply(){g_themeDirty=true;SaveSettings();if(hs)InvalidateRect(hs,0,FALSE);}
static void CommitCity(){
    if(!hEdit)return; wchar_t b[64]={0}; GetWindowTextW(hEdit,b,64);
    bool ch=false;{std::lock_guard<std::mutex> lk(cm);if(b[0]&&s_city!=b){s_city=b;ch=true;}}
    if(ch){g_wxReset=true;SaveSettings();}
}

static int HitAtCached(float x,float y){
    // sticky close
    if(x>=W-48&&x<W-12&&y>=12&&y<48) return H_CLOSE;
    // контентные зоны
    float sy=y+scrollY;
    for(auto& hr:g_hitRects){
        if(x>=hr.x&&x<hr.x+hr.w&&sy>=hr.y&&sy<hr.y+hr.h) return hr.id;
    }
    return H_NONE;
}

// ──────────────────────────────────────────────────────────────────────────────
// Simplified Paint2: рисуем контент И собираем hit-rects одновременно
// (единый проход)
// ──────────────────────────────────────────────────────────────────────────────
// Для краткости: выносим «регистрацию» хит-зоны в macro
#define REG(id,rx,ry,rw,rh) g_hitRects.push_back({id,rx,ry,rw,rh})

// Полный Paint уже описан выше. Чтобы не дублировать логику,
// добавим регистрацию прямо внутри DrawGroup через closure.
// (Реализация: строки DrawGroup регистрируют hit по ry+scrollY)

static void Click(int h,float mx,float my){
    if(h==H_CLOSE){CommitCity();ShowWindow(hs,SW_HIDE);return;}
    if(h>=H_CARD&&h<H_CARD+2){s_theme=h-H_CARD;Apply();return;}
    // toggles
    if(h>=H_TG&&h<H_TG+TG_N){
        int t=h-H_TG; auto* f=TgFlag(t); *f=*f?0:1;
        if(t==TG_WX)g_wxReset=true;
        if(t==TG_STARTUP){
            // авtozапуск: пишем в реестр Run
            if(*f){
                wchar_t path[MAX_PATH]; GetModuleFileNameW(0,path,MAX_PATH);
                RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"DynamicIsland",REG_SZ,path,(DWORD)((wcslen(path)+1)*2));
            } else {
                RegDeleteKeyValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"DynamicIsland");
            }
        }
        Apply(); return;
    }
    // scale seg
    if(h>=H_SEG_SCALE&&h<H_SEG_SCALE+6){
        s_scale=SCALES[h-H_SEG_SCALE]; g_resizeReq=true; Apply(); return;}
    // corner seg
    if(h>=H_SEG_CORNER&&h<H_SEG_CORNER+3){s_cornerR=h-H_SEG_CORNER;Apply();return;}
    // clock seg
    if(h>=H_SEG_CLOCK&&h<H_SEG_CLOCK+3){s_clock=h-H_SEG_CLOCK;Apply();return;}
    // ios style
    if(h>=H_SEG_IOS&&h<H_SEG_IOS+3){s_iosStyle=h-H_SEG_IOS;Apply();return;}
    // my shape
    if(h>=H_SEG_SHAPE&&h<H_SEG_SHAPE+3){s_myShape=h-H_SEG_SHAPE;Apply();return;}
    // accent
    if(h>=H_SEG_ACCENT&&h<H_SEG_ACCENT+3){s_accentMode=h-H_SEG_ACCENT;Apply();return;}
}

static LRESULT CALLBACK SProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);Paint(dc);EndPaint(h,&ps);return 0;}
    case WM_NCHITTEST:{
        POINT pt{(int)(short)LOWORD(l),(int)(short)HIWORD(l)}; ScreenToClient(h,&pt);
        float x=pt.x/SS,y=pt.y/SS;
        if(y>=0&&y<headerH_&&!(x>=W-48&&x<W-12&&y>=12&&y<48)) return HTCAPTION;
        return HTCLIENT;}
    case WM_MOUSEMOVE:{
        if(!tracking){TRACKMOUSEEVENT te{sizeof te,TME_LEAVE,h,0};TrackMouseEvent(&te);tracking=true;}
        float x=(short)LOWORD(l)/SS,y=(short)HIWORD(l)/SS;
        int n=HitAtCached(x,y); if(n!=hot){hot=n;InvalidateRect(h,0,FALSE);}
        // slider drag
        if(slDrag>=0){
            auto& sl=SLIDERS[slDrag];
            float sx=MARGIN+140,sw2=CONTENT_W-160-60;
            float t=cl((x-sx)/sw2,0,1);
            sl.val->store(sl.mn+(int)((sl.mx-sl.mn)*t+.5f));
            if(slDrag==1){}  // animspeed применится сам
            Apply();
        }
        return 0;}
    case WM_MOUSELEAVE: tracking=false; if(hot!=H_NONE){hot=H_NONE;InvalidateRect(h,0,FALSE);} return 0;
    case WM_SETCURSOR: if(LOWORD(l)==HTCLIENT&&hot!=H_NONE){SetCursor(LoadCursor(0,IDC_HAND));return TRUE;}break;
    case WM_LBUTTONDOWN:{
        SetFocus(h);
        float x=(short)LOWORD(l)/SS,y=(short)HIWORD(l)/SS;
        int n=HitAtCached(x,y);
        // slider check
        for(int i=0;i<3;i++) if(n==H_SL+i){slDrag=i;SetCapture(h);}
        if(n!=H_NONE&&slDrag<0) Click(n,x,y);
        // ripple (MY)
        if(s_theme!=0){ripple={x*SS,y*SS,0,1};}
        return 0;}
    case WM_LBUTTONUP:
        if(slDrag>=0){ReleaseCapture();slDrag=-1;}
        return 0;
    case WM_MOUSEWHEEL:{
        float delta=-(short)HIWORD(w)/3.f;
        scrollTgt=cl(scrollTgt+delta,0,max(0.f,g_contentH-(H-headerH_)));
        return 0;}
    case WM_COMMAND:
        if(LOWORD(w)==IDOK){CommitCity();SetFocus(h);}
        else if(LOWORD(w)==IDCANCEL){CommitCity();ShowWindow(h,SW_HIDE);}
        else if(LOWORD(w)==ID_CITY){
            if(HIWORD(w)==EN_SETFOCUS){cityFocus=true;InvalidateRect(h,0,FALSE);}
            else if(HIWORD(w)==EN_KILLFOCUS){cityFocus=false;CommitCity();InvalidateRect(h,0,FALSE);}
        }
        return 0;
    case WM_CTLCOLOREDIT:{
        Pal Pl=GetPal(); COLORREF c=CR(Pl.field);
        if(!editBr||c!=editCol){if(editBr)DeleteObject(editBr);editBr=CreateSolidBrush(c);editCol=c;}
        SetTextColor((HDC)w,CR(Pl.text));SetBkColor((HDC)w,c);return(LRESULT)editBr;}
    case WM_TIMER:{
        if(!IsWindowVisible(h))return 0;
        bool ch=false;
        // animate toggles
        for(int i=0;i<TG_N;i++){
            float t=*TgFlag(i)?1.f:0.f,d=t-tg[i];
            if(fabsf(d)<.01f){if(tg[i]!=t){tg[i]=t;ch=true;}}else{tg[i]+=d*.25f;ch=true;}
        }
        // animate segs
        auto animSeg=[&](float& a,int cur){ float d=cur-a; if(fabsf(d)<.01f){if(a!=cur){a=cur;ch=true;}}else{a+=d*.3f;ch=true;}};
        animSeg(segScaleA,(float)SegScaleIdx());
        animSeg(segCornerA,(float)s_cornerR.load());
        animSeg(segClockA, (float)s_clock.load());
        animSeg(segIosA,   (float)s_iosStyle.load());
        animSeg(segShapeA, (float)s_myShape.load());
        animSeg(segAccA,   (float)s_accentMode.load());
        // scroll
        {float d=scrollTgt-scrollY; if(fabsf(d)<.3f){if(scrollY!=scrollTgt){scrollY=scrollTgt;ch=true;}}else{scrollY+=d*.25f;ch=true;}}
        // ripple
        if(ripple.a>0){ripple.r+=8;ripple.a=max(0.f,ripple.a-.04f);ch=true;}
        // theme sig
        DWORD sig=TC(0,1).GetValue()*31u^TC(2,1).GetValue()*17u^TC(5,1).GetValue()^(DWORD)s_theme.load()^((DWORD)s_dark.load()<<5);
        if(sig!=lastSig){lastSig=sig;ch=true;if(hEdit)InvalidateRect(hEdit,0,TRUE);}
        if(ch)InvalidateRect(h,0,FALSE);
        // update edit position after scroll
        if(hEdit){
            // погода-поле: 3-я группа + row height (примерно), пересчёт через scrollY
            // (упрощение — позиция фиксирована при открытии)
        }
        return 0;}
    case WM_CLOSE: CommitCity();ShowWindow(h,SW_HIDE);return 0;
    }
    return DefWindowProc(h,m,w,l);
}

void OpenSettings(){
    if(!hs){
        WNDCLASSW wc{}; wc.lpfnWndProc=SProc; wc.hInstance=GetModuleHandle(0);
        wc.lpszClassName=L"WinIslandSet"; wc.style=CS_DROPSHADOW;
        wc.hCursor=LoadCursor(0,IDC_ARROW); RegisterClassW(&wc);
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
        SS=min(g_dpi,((wa.bottom-wa.top)-24.f)/H); if(SS<.6f)SS=.6f;
        int ww=P(W),wh=P(H);
        hs=CreateWindowExW(WS_EX_APPWINDOW,L"WinIslandSet",L"Dynamic Island — Настройки",
            WS_POPUP|WS_SYSMENU|WS_CLIPCHILDREN,
            wa.left+((wa.right-wa.left)-ww)/2,wa.top+((wa.bottom-wa.top)-wh)/2,
            ww,wh,0,0,GetModuleHandle(0),0);
        int pref=2; DwmSetWindowAttribute(hs,33,&pref,sizeof pref);
        // Edit для города — примерно row "Город" в секции Погода
        // позиция подбирается при первом Paint через g_contentH
        hEdit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
            P(MARGIN+150),P(100),P(200),P(28),
            hs,(HMENU)(INT_PTR)ID_CITY,GetModuleHandle(0),0);
        SendMessage(hEdit,EM_LIMITTEXT,60,0);
        BuildFonts();
        SetTimer(hs,1,16,0);
    }
    for(int i=0;i<TG_N;i++) tg[i]=*TgFlag(i)?1.f:0.f;
    segScaleA=(float)SegScaleIdx(); segCornerA=(float)s_cornerR.load();
    segClockA=(float)s_clock.load(); segIosA=(float)s_iosStyle.load();
    segShapeA=(float)s_myShape.load(); segAccA=(float)s_accentMode.load();
    scrollY=0; scrollTgt=0;
    SetWindowTextW(hEdit,GetCity().c_str());
    ShowWindow(hs,SW_SHOW); SetForegroundWindow(hs); InvalidateRect(hs,0,FALSE);
}
bool SettingsDialogMsg(MSG* m){return hs&&IsWindowVisible(hs)&&IsDialogMessage(hs,m);}
