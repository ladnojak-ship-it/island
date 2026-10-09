#include "icons.h"
#include "svgpath.h"

struct GpSink {
    GraphicsPath* p; float s, tx, ty, cx = 0, cy = 0;
    PointF P(float x, float y) { return PointF(x * s + tx, y * s + ty); }
    void move(float x, float y) { p->StartFigure(); cx = x; cy = y; }
    void line(float x, float y) { p->AddLine(P(cx, cy), P(x, y)); cx = x; cy = y; }
    void cubic(float a, float b, float c, float d, float e, float f) { p->AddBezier(P(cx, cy), P(a, b), P(c, d), P(e, f)); cx = e; cy = f; }
    void close() { p->CloseFigure(); }
};
static GraphicsPath* g_cache[IC_N][3];

static GraphicsPath* Get(int id, int k) {
    GraphicsPath*& gp = g_cache[id][k];
    if (!gp) {
        gp = new GraphicsPath(FillModeWinding);
        const IconPart& pt = ICONS[id].p[k];
        GpSink s{ gp, pt.s, pt.tx, pt.ty }; ParseSvgPath(pt.d, s);
    }
    return gp;
}

void DrawIcon(Graphics& g, int id, float cx, float cy, float size, Color c, int style) {
    if (style == 2) { if (id == IC_PAUSE) id = IC_PAUSE_I; else if (id == IC_PREV) id = IC_PREV_I; else if (id == IC_NEXT) id = IC_NEXT_I; }
    const IconDef& ic = ICONS[id];
    GraphicsState st = g.Save();
    g.TranslateTransform(cx, cy); g.ScaleTransform(size / 24.f, size / 24.f); g.TranslateTransform(-12, -12);
    SolidBrush br(c); float al = c.GetA() / 255.f;
    for (int k = 0; k < ic.n; k++) {
        const IconPart& pt = ic.p[k]; GraphicsPath* p = Get(id, k);
        if (pt.sw > 0) {
            Pen pen(c, pt.sw); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
            g.DrawPath(&pen, p);
        } else {
            g.FillPath(&br, p);
            if (style > 0) {   // скругляем углы обводкой того же цвета; при прозрачности утоньшаем, чтобы не было двойного наложения
                Pen pen(c, (style == 2 ? 1.5f : .8f) * al * al); pen.SetLineJoin(LineJoinRound);
                if (al > .05f) g.DrawPath(&pen, p);
            }
        }
    }
    g.Restore(st);
}
