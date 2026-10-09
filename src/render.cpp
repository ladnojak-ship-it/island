#include "render.h"
#include "settings.h"
#include "theme.h"
#include "anim.h"
#include "fonts.h"
#include "icons.h"
#include "gfx.h"

int g_canvasLeft = 0;
std::atomic<bool> g_resizeReq{ false };
static HWND hw;
static HDC mdc = nullptr; static HBITMAP hbm = nullptr; static void* bits;
static Bitmap* bmp = nullptr; static Graphics* G = nullptr;
static int curW = 0, curH = 0;
static Font *fT=nullptr,*fA=nullptr,*fS=nullptr,*fC=nullptr;
static int fontTheme=-1,fontGen=0;
static StringFormat *sfN,*sfC_,*sfF,*sfM;
static TextureBrush* artBrush=nullptr; static Bitmap* artFor=nullptr;
static constexpr float ART_PX=256.f;

// ── ripple state (MY) ────────────────────────────────────────────────────────
static float rRipX=0,rRipY=0,rRipR=0,rRipA=0;

static StringFormat* MkSF(StringAlignment a){
    StringFormat* f=new StringFormat(StringFormatFlagsNoWrap);
    f->SetAlignment(a); f->SetLineAlignment(StringAlignmentCenter);
    f->SetTrimming(StringTrimmingEllipsisCharacter); return f;
}
static void BuildFonts(){
    int th=s_theme?1:0; if(th==fontTheme)return;
    delete fT; delete fA; delete fS; delete fC;
    // iOS: SF Pro Display weights; MY: Google Sans / Roboto Flex
    fT=MkFont(th,18.f,true); fA=MkFont(th,14.5f,false);
    fS=MkFont(th,13.f,false); fC=MkFont(th,15.f,true);
    fontTheme=th; fontGen++;
}
static bool MakeSurface(){
    int w=(int)ceilf(CW*g_S),h=(int)ceilf(CH*g_S);
    delete G; G=nullptr; delete bmp; bmp=nullptr;
    BITMAPINFO bi{}; bi.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};
    HBITMAP nb=CreateDIBSection(0,&bi,DIB_RGB_COLORS,&bits,0,0); if(!nb)return false;
    if(!mdc) mdc=CreateCompatibleDC(0);
    SelectObject(mdc,nb); if(hbm)DeleteObject(hbm); hbm=nb; curW=w; curH=h;
    bmp=new Bitmap(w,h,w*4,PixelFormat32bppPARGB,(BYTE*)bits);
    G=new Graphics(bmp);
    G->SetSmoothingMode(SmoothingModeHighQuality);
    G->SetPixelOffsetMode(PixelOffsetModeHighQuality);
    G->SetInterpolationMode(InterpolationModeHighQualityBicubic);
    G->SetCompositingQuality(CompositingQualityHighQuality);
    G->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g_canvasLeft=(GetSystemMetrics(SM_CXSCREEN)-w)/2;
    return true;
}
bool RenderInit(HWND h){
    hw=h; if(!MakeSurface())return false;
    sfN=MkSF(StringAlignmentNear); sfC_=MkSF(StringAlignmentCenter);
    sfF=MkSF(StringAlignmentFar);
    sfM=StringFormat::GenericTypographic()->Clone(); sfM->SetLineAlignment(StringAlignmentCenter);
    BuildFonts(); return true;
}
bool RenderResize(){
    float sc=s_scale.load()/100.f; g_S=g_dpi*sc;
    if(artBrush){delete artBrush;artBrush=nullptr;artFor=nullptr;}
    return MakeSurface();
}

Lay LayoutFor(float pw){
    Lay l; l.px=(CW-pw)/2; l.py=8; l.pw=pw;
    float x=l.px,y=l.py,cx=x+pw/2;
    l.prog=RectF(x+68,y+102,pw-136,4); l.vb=RectF(x+60,y+180,pw-130,5);
    l.mute=PointF(x+34,y+182.5f);
    l.prev=PointF(cx-76,y+140); l.play=PointF(cx,y+140); l.next=PointF(cx+76,y+140);
    for(int i=0;i<5;i++) l.launch[i]=PointF(cx+(i-2)*52,y+222);
    return l;
}

// ── helpers ───────────────────────────────────────────────────────────────────
static void Txt(Graphics& g,const wchar_t* s,Font* f,RectF r,Color c,StringAlignment a=StringAlignmentNear){
    SolidBrush b(c);
    g.DrawString(s,-1,f,r,a==StringAlignmentNear?sfN:a==StringAlignmentCenter?sfC_:sfF,&b);
}
static void Bar(Graphics& g,float x,float y,float w,float h,float fr,Color bg,Color fg){
    GraphicsPath p; RR(p,RectF(x,y,w,h),h/2); SolidBrush b(bg); g.FillPath(&b,&p);
    if(fr>0.001f){GraphicsPath q;RR(q,RectF(x,y,max(h,w*fr),h),h/2);SolidBrush f(fg);g.FillPath(&f,&q);}
}
// M3 WaveBar (Material You progress)
static void WaveBar(Graphics& g,float x,float cy,float w,float fr,float amp,double t,float th,Color bg,Color fg,float hv){
    float xf=x+w*fr,gap=4+2*hv;
    Pen tp(bg,th); tp.SetStartCap(LineCapRound); tp.SetEndCap(LineCapRound);
    if(xf+gap<x+w) g.DrawLine(&tp,xf+gap,cy,x+w,cy);
    static PointF pts[200]; int n=0; float end=max(x,xf-gap);
    for(float px=x;px<=end&&n<199;px+=2.5f) pts[n++]=PointF(px,cy+amp*sinf((px-x)*.42f-(float)t*5.f));
    if(n>=2){Pen wp(fg,th);wp.SetStartCap(LineCapRound);wp.SetEndCap(LineCapRound);wp.SetLineJoin(LineJoinRound);g.DrawLines(&wp,pts,n);}
    else if(fr>.001f){Pen wp(fg,th);wp.SetStartCap(LineCapRound);wp.SetEndCap(LineCapRound);g.DrawLine(&wp,x,cy,max(x+.1f,end),cy);}
    float hh=18+4*hv; GraphicsPath hp; RR(hp,RectF(xf-2,cy-hh/2,4,hh),2); SolidBrush hb(fg); g.FillPath(&hb,&hp);
}

// ── WASAPI EQ bars ────────────────────────────────────────────────────────────
// Компактный (4 полосы, в пилюле)
static void EQCompact(Graphics& g,float x,float cy,float amp,double t,float mh,Color c){
    const int idx[4]={1,2,4,5};
    for(int i=0;i<4;i++){
        float band=s_eq?g_eqBands[idx[i]]:0.f;
        float idle=3.f+2.f*sinf((float)t*(4+i*1.7f)+i*1.3f);
        float h=idle+(mh-idle)*amp*(0.5f+0.5f*band);
        h=max(2.f,h);
        GraphicsPath p; RR(p,RectF(x+i*5,cy-h/2,3,h),1.5f);
        SolidBrush b(c); g.FillPath(&b,&p);
    }
}
// Раскрытый (8 полос, справа от обложки)
static void EQFull(Graphics& g,float cx,float cy,float amp,double t,float mh,Color c,float alpha){
    const int BARS=8; float barW=3.f,gap=2.f;
    float totalW=BARS*barW+(BARS-1)*gap;
    float sx=cx-totalW/2;
    for(int i=0;i<BARS;i++){
        float band=s_eq?g_eqBands[i]:0.f;
        float idle=2.f+1.5f*sinf((float)t*(2.f+i*.7f)+i*1.1f);
        float h=idle+(mh-idle)*amp*band;
        h=max(2.f,h);
        BYTE aa=(BYTE)(alpha*255);
        Color bc(aa,c.GetR(),c.GetG(),c.GetB());
        GraphicsPath p; RR(p,RectF(sx+i*(barW+gap),cy-h/2,barW,h),barW/2);
        SolidBrush b(bc); g.FillPath(&b,&p);
    }
}

static void ScaleAt(Graphics& g,float cx,float cy,float s){
    g.TranslateTransform(cx,cy); g.ScaleTransform(s,s); g.TranslateTransform(-cx,-cy);
}
static float stag(float a,int i){return ease((a-i*.07f)/.5f);}

static Color HSV(float h,float s,float v){
    float c=v*s,x=c*(1-fabsf(fmodf(h/60,2)-1)),m=v-c,r=0,g=0,b=0;
    switch((int)(h/60)%6){case 0:r=c;g=x;break;case 1:r=x;g=c;break;case 2:g=c;b=x;break;
        case 3:g=x;b=c;break;case 4:r=x;b=c;break;default:r=c;b=x;}
    return Color(255,(BYTE)((r+m)*255),(BYTE)((g+m)*255),(BYTE)((b+m)*255));
}
static std::wstring Tm(double s){
    if(s<0)s=0; wchar_t b[16];
    swprintf(b,16,L"%d:%02d",(int)s/60,(int)s%60); return b;
}

// бегущая строка
static void Marquee(Graphics& g,int slot,const std::wstring& s,Font* f,RectF r,Color c,double t,float xoff){
    static std::wstring tt[2]; static float tw[2]; static int gen[2]={-1,-1};
    if(tt[slot]!=s||gen[slot]!=fontGen){
        tt[slot]=s; gen[slot]=fontGen;
        RectF b; g.MeasureString(s.c_str(),-1,f,PointF(0,0),sfN,&b); tw[slot]=b.Width;
    }
    float w=tw[slot],off=0; bool scroll=w>r.Width+1;
    if(scroll){
        float over=w-r.Width+8,travel=over/32.f,cyc=2.4f+2*travel,ph=(float)fmod(t,cyc);
        off=ph<1.2f?0:ph<1.2f+travel?(ph-1.2f)*32.f:ph<2.4f+travel?over:over-(ph-2.4f-travel)*32.f;
    }
    GraphicsState st=g.Save(); g.SetClip(r,CombineModeIntersect);
    if(scroll&&off>0) g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    SolidBrush b(c);
    g.DrawString(s.c_str(),-1,f,RectF(r.X+xoff-off,r.Y,max(w+16,r.Width),r.Height),sfN,&b);
    g.Restore(st);
}

static int WxIcon(int c){
    if(c==0)return IC_SUN; if(c<=3)return IC_CLOUD;
    if(c==45||c==48)return IC_FOG;
    if((c>=71&&c<=77)||c==85||c==86)return IC_SNOW; if(c>=95)return IC_STORM; return IC_RAIN;
}
static const int LG[5]={IC_FOLDER,IC_TERM,IC_CALC,IC_CAM,IC_GEAR};

// ── Squircle helper (MY shape=0) ─────────────────────────────────────────────
static void Squircle(GraphicsPath& p,float x,float y,float w,float h){
    // аппроксимация суперэллипса n=4 через кубические кривые
    float rx=w/2,ry=h/2,cx=x+rx,cy=y+ry;
    float k=0.6518f; // контрольная точка
    p.AddBezier(cx,y, cx+rx*k,y, x+w,cy-ry*k, x+w,cy);
    p.AddBezier(x+w,cy, x+w,cy+ry*k, cx+rx*k,y+h, cx,y+h);
    p.AddBezier(cx,y+h, cx-rx*k,y+h, x,cy+ry*k, x,cy);
    p.AddBezier(x,cy, x,cy-ry*k, cx-rx*k,y, cx,y);
    p.CloseFigure();
}

// Получить радиус скруглений по настройке
static float GetRad(float ph){
    switch(s_cornerR.load()){
    case 0: return min(ph/2,34.f);
    case 1: return min(ph/2,18.f);
    case 2: return 6.f;
    default: return min(ph/2,34.f);
    }
}

// ── Dominant color из обложки для iOS-тинта ──────────────────────────────────
static Color g_artDom={255,10,132,255}; // кэш доминантного цвета
static Bitmap* g_artDomFor=nullptr;
static Color GetArtDom(Bitmap* art){
    if(!art||art==g_artDomFor) return g_artDom;
    g_artDomFor=art;
    // сжимаем до 8x8 и берём средний насыщенный пиксель
    Bitmap sm(8,8,PixelFormat32bppARGB);
    {Graphics gg(&sm); gg.SetInterpolationMode(InterpolationModeBilinear);
     gg.DrawImage(art,0,0,8,8);}
    BitmapData bd; Rect rc(0,0,8,8);
    if(sm.LockBits(&rc,ImageLockModeRead,PixelFormat32bppARGB,&bd)!=Ok) return g_artDom;
    float br=0,bg=0,bb=0,bw=0;
    for(int y=0;y<8;y++) for(int x=0;x<8;x++){
        BYTE* q=(BYTE*)bd.Scan0+y*bd.Stride+x*4;
        float b_=q[0]/255.f,g_=q[1]/255.f,r_=q[2]/255.f;
        float mx=max(r_,max(g_,b_)),mn=min(r_,min(g_,b_));
        float s=(mx>0)?(mx-mn)/mx:0; float v=mx;
        if(s<.3f||v<.2f)continue;
        float w=s*v;
        br+=r_*w; bg+=g_*w; bb+=b_*w; bw+=w;
    }
    sm.UnlockBits(&bd);
    if(bw>0){g_artDom=Color(255,(BYTE)(cl(br/bw,0,1)*255),(BYTE)(cl(bg/bw,0,1)*255),(BYTE)(cl(bb/bw,0,1)*255));}
    return g_artDom;
}

// ── КАДР ─────────────────────────────────────────────────────────────────────
void RenderFrame(const View& v){
    BuildFonts();
    Graphics& g=*G;
    const bool MY  =(s_theme!=0);
    const bool lite=(s_lite!=0);
    const bool tint=(s_iosTint!=0)&&!MY;
    const Media& m=v.m;
    const int IS=MY?1:2;

    float slideOff=v.slideY*g_S;
    float bumpScale=1.0f+0.07f*v.screenshotBump;

    float pw=max(v.pw,24.f)*bumpScale, ph=max(v.ph,24.f)*bumpScale;
    Lay L=LayoutFor(pw);
    float px=L.px,py=L.py;

    // радиус: MY shape=0 → squircle (особо обрабатываем), иначе RR
    float rad;
    bool useSquircle=(MY&&s_myShape==0&&ph>50);
    if(MY){
        switch(s_myShape.load()){
        case 0: rad=min(ph/2,28.f); break;  // squircle аппрокс через большой radius
        case 1: rad=min(ph/2,20.f); break;
        default: rad=min(ph/2,34.f);
        }
    } else {
        rad=GetRad(ph);
    }

    // тинт-акцент: iOS тинт из обложки
    Color accentC=TC(2,1);
    if(tint&&v.art&&m.has){
        Color dom=GetArtDom(v.art);
        accentC=Mix(TC(2,1),dom,0.7f);
    }

    // dirty rect
    float mg=lite?4.f:22.f;
    int x0=(int)floorf(max(0.f,px-mg));
    int x1=(int)ceilf(min((float)CW,px+pw+mg));
    int y1=(int)ceilf(min((float)CH,py+ph+mg+8));
    int sx0=(int)(x0*g_S);
    int sw=min(curW-sx0,(int)ceilf((x1-x0)*g_S)+1);
    int sh=min(curH,(int)ceilf(y1*g_S)+1);

    g.ResetTransform(); g.ResetClip();
    g.SetCompositingMode(CompositingModeSourceCopy);
    {SolidBrush clr(Color(0,0,0,0)); g.FillRectangle(&clr,sx0,0,sw,sh);}
    g.SetCompositingMode(CompositingModeSourceOver);
    g.ScaleTransform(g_S,g_S);
    g.TranslateTransform(0.f,v.slideY);

    if(v.art!=artFor){
        delete artBrush;
        artBrush=v.art?new TextureBrush(v.art,WrapModeClamp):nullptr;
        artFor=v.art;
    }
    const bool hasArt=m.has&&artBrush&&s_art;

    // ── тень ─────────────────────────────────────────────────────────────────
    if(!lite&&s_shadow){
        int layers=(s_shadow==2)?5:3;
        for(int i=layers;i>=1;i--){
            float spread=i*2.5f;
            GraphicsPath sp; RR(sp,RectF(px-spread,py+i*1.8f,pw+spread*2,ph+i*2.5f),rad+spread);
            SolidBrush sb(Color(MY?8:12,0,0,0)); g.FillPath(&sb,&sp);
        }
    }

    // ── MY Elevation tonal surface ────────────────────────────────────────────
    // M3: поверх базового bg, накладываем tonal overlay по уровню elevation
    if(MY&&s_myElevation.load()>0){
        float elev=s_myElevation.load()==2?0.12f:0.08f;
        GraphicsPath ep; RR(ep,RectF(px-1,py-1,pw+2,ph+2),rad+1);
        SolidBrush eb(TC(2,elev*0.5f)); g.FillPath(&eb,&ep);
    }

    // ── фон острова ──────────────────────────────────────────────────────────
    GraphicsPath path;
    if(useSquircle) Squircle(path,px,py,pw,ph);
    else            RR(path,RectF(px,py,pw,ph),rad);

    if(!MY){
        // iOS: чисто чёрный с лёгким градиентом в нижней части
        BYTE bgA=(BYTE)(s_opacity.load()*255/100);
        Color bg0=Color(bgA,12,12,14);
        Color bg1=Color(bgA,22,22,26);
        LinearGradientBrush lb(PointF(px,py),PointF(px,py+ph),bg0,bg1);
        g.FillPath(&lb,&path);
    } else {
        // MY: surface с tonal overlay
        Color bgC=TC(0,s_opacity.load()/100.f);
        SolidBrush bk(bgC); g.FillPath(&bk,&path);
        if(s_myElevation.load()>0){
            // M3 tonal: слой акцента поверх
            SolidBrush ton(TC(2,.05f)); g.FillPath(&ton,&path);
        }
    }

    // ── кант ─────────────────────────────────────────────────────────────────
    if(s_border.load()>0){
        float bw=0.8f;
        Color bc;
        if(!MY){
            // iOS: тонкий белый кант сверху (как отражение)
            bc=s_border==2?Mix(accentC,Color(255,255,255,255),.3f):Color(20,255,255,255);
            // Рисуем только верхнюю половину (highlight)
            Pen edge(bc,bw); g.DrawPath(&edge,&path);
        } else {
            // MY: outline token
            bc=TC(1,s_border==2?0.35f:0.12f);
            Pen edge(bc,bw); g.DrawPath(&edge,&path);
        }
    }

    // ── glow ─────────────────────────────────────────────────────────────────
    if(!lite&&s_glow&&v.glow>.01f){
        Color glowC=tint?accentC:TC(2,1);
        Pen p1(Color((BYTE)(v.glow*25),glowC.GetR(),glowC.GetG(),glowC.GetB()),8.f);
        Pen p2(Color((BYTE)(v.glow*140),glowC.GetR(),glowC.GetG(),glowC.GetB()),1.5f);
        g.DrawPath(&p1,&path); g.DrawPath(&p2,&path);
    }

    g.SetClip(&path);

    // ── iOS: compact pill style overlay (iosStyle > 0) ───────────────────────
    if(!MY&&s_iosStyle.load()==1&&!v.m.has){
        // компакт: тинт из обложки на фон
        if(tint&&v.art){
            Color dom=GetArtDom(v.art);
            SolidBrush tb(Color(18,dom.GetR(),dom.GetG(),dom.GetB()));
            g.FillRectangle(&tb,(int)px,(int)py,(int)pw,(int)ph);
        }
    }

    float ae=cl((v.fade-.3f)/.7f,0,1), ca=cl(1-v.fade*2.5f,0,1);
    unsigned hh=5381; for(wchar_t c:m.title) hh=hh*33+c;
    Color c1=HSV((float)(hh%360),.65f,.95f), c2=HSV((float)((hh+40)%360),.8f,.5f);

    // Обложка / плейсхолдер
    auto Art=[&](RectF r,float rd,float a){
        GraphicsPath p; RR(p,r,rd);
        if(hasArt){
            artBrush->ResetTransform();
            artBrush->ScaleTransform(r.Width/ART_PX,r.Height/ART_PX);
            artBrush->TranslateTransform(r.X,r.Y,MatrixOrderAppend);
            g.FillPath(artBrush,&p);
            if(a<1){SolidBrush ov(TC(0,1-a));g.FillPath(&ov,&p);}
        } else if(m.has){
            // iOS: цвет из названия трека; MY: acc gradient
            Color a1=!MY?Al(c1,a):TC(2,a);
            Color a2=!MY?Al(c2,a):TC(5,a);
            LinearGradientBrush lb(PointF(r.X,r.Y),PointF(r.X+r.Width,r.Y+r.Height),a1,a2);
            g.FillPath(&lb,&p);
        } else {
            SolidBrush sb(TC(MY?5:1,MY?a:a*.1f)); g.FillPath(&sb,&p);
        }
    };

    // ── КОМПАКТНЫЙ режим ─────────────────────────────────────────────────────
    if(ca>0.01f){
        float cy_=py+ph/2; wchar_t b[64];
        GraphicsState st=g.Save();

        if(v.hud){
            ScaleAt(g,px+26,cy_,.6f+.4f*v.hudPop);
        }
        Color ac2=tint?accentC:TC(2,ca);

        if(v.hud==1){
            // громкость
            DrawIcon(g,(v.mute||v.volDisp<.005f)?IC_MUTE:IC_VOL,px+26,cy_,22,TC(1,ca),IS);
            g.Restore(st);
            // iOS: bar с акцентом
            Bar(g,px+50,cy_-(MY?3.f:2.5f),pw-50-62,MY?6.f:5.f,
                v.mute?0:v.volDisp, TC(1,ca*.2f), ac2);
            swprintf(b,64,L"%d",(int)(v.volDisp*100+.5f));
            Txt(g,b,fC,RectF(px+pw-56,cy_-11,38,22),TC(1,ca),StringAlignmentFar);
        } else if(v.hud==2){
            // батарея
            Color gc=Color((BYTE)(ca*255),48,209,88);
            DrawIcon(g,IC_BOLT,px+26,cy_,22,v.sys.ac?gc:TC(1,ca),IS); g.Restore(st);
            Txt(g,v.sys.ac?L"Зарядка":L"От батареи",fC,RectF(px+46,cy_-11,160,22),TC(1,ca));
            swprintf(b,64,L"%d%%",v.sys.bat);
            Txt(g,b,fC,RectF(px+pw-76,cy_-11,58,22),v.sys.ac?gc:TC(1,ca),StringAlignmentFar);
        } else if(v.hud==3){
            DrawIcon(g,IC_RAIN,px+26,cy_,24,ac2,IS); g.Restore(st);
            Txt(g,v.hudText.c_str(),fC,RectF(px+50,cy_-11,pw-66,22),TC(1,ca));
        } else if(v.hud==4){
            // iOS: bell с тинтом
            DrawIcon(g,IC_BELL,px+26,cy_,22,ac2,IS); g.Restore(st);
            Txt(g,v.hudText.c_str(),fC,RectF(px+50,cy_-11,pw-66,22),TC(1,ca));
        } else if(m.has){
            g.Restore(st); st=g.Save();
            // iOS: обложка с тинтом
            ScaleAt(g,px+24,cy_,.8f+.2f*backOut(v.trackAnim));
            float artRad=!MY?6.f:9.f;
            Art(RectF(px+12,cy_-12,24,24),artRad,ca);
            if(tint&&hasArt){
                // тонкий border из dom-цвета вокруг обложки
                GraphicsPath ap; RR(ap,RectF(px+12,cy_-12,24,24),artRad);
                Pen ab(Color(60,accentC.GetR(),accentC.GetG(),accentC.GetB()),1.f);
                g.DrawPath(&ab,&ap);
            }
            g.Restore(st);
            // EQ справа
            EQCompact(g,px+pw-36,cy_,v.eqAmp,v.now,18,ac2);
        } else {
            g.Restore(st);
            // iOS: время
            SYSTEMTIME t; GetLocalTime(&t);
            if(s_clock==2){
                // 12-hour
                int h12=t.wHour%12; if(!h12)h12=12;
                swprintf(b,64,L"%d:%02d",h12,t.wMinute);
            } else if(s_clock==1){
                swprintf(b,64,L"%02d:%02d:%02d",t.wHour,t.wMinute,t.wSecond);
            } else {
                swprintf(b,64,L"%02d:%02d",t.wHour,t.wMinute);
            }
            Txt(g,b,fC,RectF(px,cy_-11,pw,22),TC(1,ca),StringAlignmentCenter);
        }
    }

    // ── РАСКРЫТЫЙ режим ──────────────────────────────────────────────────────
    if(ae>0.01f){
        double pos=m.pos+(m.playing?(GetTickCount64()-m.t)/1000.0:0);
        if(m.dur>0&&pos>m.dur) pos=m.dur;
        std::wstring ttl=m.has?m.title:L"Ничего не играет";
        std::wstring art_=m.has?m.artist:L"Запусти музыку";
        wchar_t b[200];
        Color ac2=tint?accentC:TC(2,1);

        // ── ряд 0: обложка + название + EQ ──────────────────────────────────
        {
            float a=stag(ae,0), ta=ease(v.trackAnim), tb=ease(cl(v.trackAnim*1.3f-.3f,0,1));
            GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
            {
                GraphicsState s2=g.Save();
                ScaleAt(g,px+54,py+52,.8f+.2f*backOut(v.trackAnim));
                float artRad=!MY?14.f:20.f;
                Art(RectF(px+20,py+18,68,68),artRad,a);
                // iOS: рамка с тинтом вокруг обложки
                if(tint&&hasArt){
                    GraphicsPath ap; RR(ap,RectF(px+20,py+18,68,68),artRad);
                    Pen ab(Color(80,accentC.GetR(),accentC.GetG(),accentC.GetB()),2.f);
                    g.DrawPath(&ab,&ap);
                }
                // MY: reflection/shine overlay поверх обложки
                if(MY&&hasArt){
                    LinearGradientBrush shine(PointF(px+20,py+18),PointF(px+20,py+86),
                        Color(30,255,255,255),Color(0,255,255,255));
                    GraphicsPath ap2; RR(ap2,RectF(px+20,py+18,68,34),artRad);
                    g.FillPath(&shine,&ap2);
                }
                if(!hasArt) DrawIcon(g,IC_NOTE,px+54,py+52,32,
                    m.has?Al(Color(255,255,255,255),a):TC(MY?2:1,a*.6f),IS);
                g.Restore(s2);
            }
            Marquee(g,0,ttl,fT,RectF(px+102,py+20,pw-102-66,28),TC(1,a*ta),v.now,(1-ta)*16);
            Marquee(g,1,art_,fA,RectF(px+102,py+48,pw-102-66,24),TC(1,a*.62f*tb),v.now,(1-tb)*16);
            // 8-полосный EQ
            EQFull(g,px+pw-46,py+34,v.eqAmp,v.now,22,ac2,a);
            g.Restore(st);
        }

        // ── ряд 1: прогресс-бар ──────────────────────────────────────────────
        {
            float a=stag(ae,1), hv=v.hov[8], cy_=L.prog.Y+2;
            float fr=m.dur>0?(float)(pos/m.dur):0;
            GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
            if(MY){
                // M3 WaveBar
                WaveBar(g,L.prog.X,cy_,L.prog.Width,fr,2.4f*v.playAnim,v.now,4.5f,TC(1,a*.22f),ac2,hv);
            } else {
                // iOS: тонкая тонкая лента, утолщается при наведении
                float hgt=4+3*hv;
                Bar(g,L.prog.X,cy_-hgt/2,L.prog.Width,hgt,fr,TC(1,a*.18f),ac2);
                if(hv>.02f){
                    float r=6*hv,tx=L.prog.X+L.prog.Width*fr;
                    SolidBrush tbr(Color(255,255,255,255)); g.FillEllipse(&tbr,tx-r,cy_-r,2*r,2*r);
                }
            }
            Txt(g,Tm(pos).c_str(),fS,RectF(px+18,L.prog.Y-8,46,22),TC(1,a*.6f));
            Txt(g,Tm(m.dur).c_str(),fS,RectF(px+pw-64,L.prog.Y-8,46,22),TC(1,a*.6f),StringAlignmentFar);
            g.Restore(st);
        }

        // ── ряд 2: кнопки управления ─────────────────────────────────────────
        {
            float a=stag(ae,2);
            for(int i=0;i<3;i++){
                PointF c=i==0?L.prev:i==1?L.play:L.next;
                GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
                ScaleAt(g,c.X,c.Y,(1+.08f*v.hov[i])*(1-.14f*v.press[i]));
                if(i==1){
                    if(MY){
                        // M3 Expressive: morphing squircle → circle
                        float morphRad=23-9*v.playAnim;
                        if(s_myShape==0){
                            GraphicsPath bp; Squircle(bp,c.X-27,c.Y-23,54,46);
                            SolidBrush wb(TC(3,a)); g.FillPath(&wb,&bp);
                        } else {
                            GraphicsPath bp; RR(bp,RectF(c.X-27,c.Y-23,54,46),morphRad);
                            SolidBrush wb(TC(3,a)); g.FillPath(&wb,&bp);
                        }
                        // MY motion: inner ripple при нажатии
                        if(s_myRipple&&v.press[1]>.02f){
                            float rr=27*v.press[1];
                            SolidBrush rb(TC(4,v.press[1]*.3f));
                            g.FillEllipse(&rb,c.X-rr,c.Y-rr,rr*2,rr*2);
                        }
                    } else {
                        // iOS: SF circle button, белый фон
                        SolidBrush wb(TC(3,a)); g.FillEllipse(&wb,c.X-23,c.Y-23,46.f,46.f);
                        // iOS: тинт акцента поверх кнопки если tint
                        if(tint){
                            SolidBrush tb2(Color((BYTE)(a*30),accentC.GetR(),accentC.GetG(),accentC.GetB()));
                            g.FillEllipse(&tb2,c.X-23,c.Y-23,46.f,46.f);
                        }
                    }
                    DrawIcon(g,IC_PLAY, c.X,c.Y,28,TC(4,a*(1-v.playAnim)),IS);
                    DrawIcon(g,IC_PAUSE,c.X,c.Y,28,TC(4,a*v.playAnim),IS);
                } else {
                    if(v.hov[i]>.02f){
                        SolidBrush hb(TC(1,a*.14f*v.hov[i]));
                        if(MY){GraphicsPath hp;RR(hp,RectF(c.X-24,c.Y-22,48,44),18);g.FillPath(&hb,&hp);}
                        else g.FillEllipse(&hb,c.X-23,c.Y-23,46.f,46.f);
                        // MY ripple
                        if(MY&&s_myRipple&&v.press[i]>.02f){
                            float rr=24*v.press[i];
                            SolidBrush rb(TC(1,v.press[i]*.2f));
                            g.FillEllipse(&rb,c.X-rr,c.Y-rr,rr*2,rr*2);
                        }
                    }
                    DrawIcon(g,i==0?IC_PREV:IC_NEXT,c.X,c.Y,28,TC(1,a),IS);
                }
                g.Restore(st);
            }
        }

        // ── ряд 3: громкость ─────────────────────────────────────────────────
        {
            float a=stag(ae,3), hv=v.hov[9], cy_=L.vb.Y+2.5f;
            float vol=v.mute?0.f:v.volDisp;
            GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
            DrawIcon(g,(v.mute||v.volDisp<.005f)?IC_MUTE:IC_VOL,L.mute.X,cy_,22,TC(1,a*.85f),IS);
            Color ac2_=tint?accentC:TC(2,a);
            if(MY){
                float hgt=8+2*hv;
                Bar(g,L.vb.X,cy_-hgt/2,L.vb.Width,hgt,vol,TC(1,a*.22f),ac2_);
                // M3 thumb
                float tx=L.vb.X+L.vb.Width*vol, hhh=20+4*hv;
                GraphicsPath hp; RR(hp,RectF(tx-2,cy_-hhh/2,4,hhh),2);
                SolidBrush hb(ac2_); Pen gp(TC(0,a),2.f); g.DrawPath(&gp,&hp); g.FillPath(&hb,&hp);
            } else {
                // iOS: тонкая полоска, knob появляется при hover
                float hgt=5+3*hv;
                Bar(g,L.vb.X,cy_-hgt/2,L.vb.Width,hgt,vol,TC(1,a*.18f),ac2_);
                if(hv>.02f){
                    float r=6*hv,tx=L.vb.X+L.vb.Width*vol;
                    SolidBrush tbr(Color(255,255,255,255)); g.FillEllipse(&tbr,tx-r,cy_-r,2*r,2*r);
                }
            }
            swprintf(b,200,L"%d",(int)(v.volDisp*100+.5f));
            Txt(g,b,fS,RectF(px+pw-62,cy_-11,44,22),TC(1,a*.6f),StringAlignmentFar);
            g.Restore(st);
        }

        // ── ряд 4: быстрые кнопки ────────────────────────────────────────────
        if(s_launch) for(int i=0;i<5;i++){
            float a=stag(ae,3+i); PointF c=L.launch[i];
            GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
            ScaleAt(g,c.X,c.Y,(1+.12f*v.hov[3+i])*(1-.14f*v.press[3+i]));
            if(MY){
                // M3: tonal filled container
                if(s_myShape==0){GraphicsPath bp;Squircle(bp,c.X-19,c.Y-19,38,38);SolidBrush cb(TC(5,a));g.FillPath(&cb,&bp);}
                else {GraphicsPath bp;RR(bp,RectF(c.X-19,c.Y-19,38,38),13);SolidBrush cb(TC(5,a));g.FillPath(&cb,&bp);}
                if(s_myRipple&&v.press[3+i]>.02f){
                    float rr=19*v.press[3+i];
                    SolidBrush rb(TC(2,v.press[3+i]*.25f));
                    g.FillEllipse(&rb,c.X-rr,c.Y-rr,rr*2,rr*2);
                }
            } else {
                // iOS: круглая кнопка с полупрозрачным фоном
                SolidBrush cb(Color((BYTE)(a*70),255,255,255));
                g.FillEllipse(&cb,c.X-19,c.Y-19,38.f,38.f);
                if(tint){
                    SolidBrush tb2(Color((BYTE)(a*20),accentC.GetR(),accentC.GetG(),accentC.GetB()));
                    g.FillEllipse(&tb2,c.X-19,c.Y-19,38.f,38.f);
                }
            }
            DrawIcon(g,LG[i],c.X,c.Y,21,TC(1,a),IS); g.Restore(st);
        }

        // ── ряд 5: sys-строка ────────────────────────────────────────────────
        if(s_sysBar){
            float a=stag(ae,6); SYSTEMTIME t; GetLocalTime(&t);
            struct Seg{int ic;wchar_t t[24];float w;Color col;} sg[6]; int ns=0;
            Color gc(255,48,209,88);
            Color ac2_=tint?accentC:TC(2,1);
            if(s_wx&&v.wx.ok){sg[ns].ic=WxIcon(v.wx.code);swprintf(sg[ns].t,24,L"%d°",v.wx.temp);sg[ns].col=ac2_;ns++;}
            sg[ns].ic=-1; swprintf(sg[ns].t,24,L"CPU %d%%",(int)(v.sys.cpu+.5f)); ns++;
            sg[ns].ic=-1; swprintf(sg[ns].t,24,L"RAM %d%%",v.sys.ram); ns++;
            if(v.sys.bat>=0){sg[ns].ic=v.sys.ac?IC_BOLT:-1;sg[ns].col=gc;swprintf(sg[ns].t,24,L"BAT %d%%",v.sys.bat);ns++;}
            // время
            if(s_clock==1) swprintf(sg[ns].t,24,L"%02d:%02d:%02d",t.wHour,t.wMinute,t.wSecond);
            else if(s_clock==2){int h12=t.wHour%12;if(!h12)h12=12;swprintf(sg[ns].t,24,L"%d:%02d",h12,t.wMinute);}
            else swprintf(sg[ns].t,24,L"%02d:%02d",t.wHour,t.wMinute);
            sg[ns].ic=-1; ns++;
            float tot=0; const float gap=14,iw=18;
            for(int i=0;i<ns;i++){RectF r;g.MeasureString(sg[i].t,-1,fS,PointF(0,0),sfM,&r);sg[i].w=r.Width+(sg[i].ic>=0?iw:0);tot+=sg[i].w+(i?gap:0);}
            float x=px+(pw-tot)/2, cy_=py+(s_launch?250.f:212.f);
            GraphicsState st=g.Save(); g.TranslateTransform(0,(1-a)*10);
            for(int i=0;i<ns;i++){
                float tx=x;
                if(sg[i].ic>=0){DrawIcon(g,sg[i].ic,x+7.5f,cy_,15,Al(sg[i].col,a*.9f),IS);tx+=iw;}
                SolidBrush tb(TC(1,a*.62f)); g.DrawString(sg[i].t,-1,fS,RectF(tx,cy_-10,sg[i].w+6,20),sfM,&tb);
                x+=sg[i].w+gap;
            }
            g.Restore(st);
        }
    }

    // ── MY Ripple внутри острова ──────────────────────────────────────────────
    if(MY&&s_myRipple&&rRipA>0){
        Color rc2=Color((BYTE)(rRipA*255*.15f),TC(2,1).GetR(),TC(2,1).GetG(),TC(2,1).GetB());
        SolidBrush rb(rc2); g.FillEllipse(&rb,rRipX-rRipR,rRipY-rRipR,rRipR*2,rRipR*2);
        rRipR+=4; rRipA=max(0.f,rRipA-.05f);
    }

    g.ResetClip(); g.ResetTransform();

    POINT dst{g_canvasLeft+sx0,(int)slideOff},src{sx0,0}; SIZE sz{sw,sh};
    BLENDFUNCTION bf{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    dst.y=(int)slideOff;
    UpdateLayeredWindow(hw,NULL,&dst,&sz,mdc,&src,0,&bf,ULW_ALPHA);
}
