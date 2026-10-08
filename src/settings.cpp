#include "settings.h"
#include "theme.h"
#include "weather.h"

std::atomic<int> s_theme{ 0 }, s_dark{ 1 }, s_wx{ 1 }, s_launch{ 1 }, s_art{ 1 }, s_hud{ 1 }, s_lite{ 0 };
static std::wstring s_city = L"Москва"; static std::mutex cm;
static const wchar_t* RK = L"Software\\WinIsland";

static int rdI(const wchar_t* n, int d) { DWORD v = 0, sz = 4; return RegGetValueW(HKEY_CURRENT_USER, RK, n, RRF_RT_REG_DWORD, 0, &v, &sz) == ERROR_SUCCESS ? (int)v : d; }
static void wrI(const wchar_t* n, int v) { DWORD d = v; RegSetKeyValueW(HKEY_CURRENT_USER, RK, n, REG_DWORD, &d, 4); }
std::wstring GetCity() { std::lock_guard<std::mutex> lk(cm); return s_city; }
void LoadSettings() {
    s_theme = rdI(L"theme", 0); s_dark = rdI(L"dark", 1); s_wx = rdI(L"wx", 1);
    s_launch = rdI(L"launch", 1); s_art = rdI(L"art", 1); s_hud = rdI(L"hud", 1); s_lite = rdI(L"lite", 0);
    wchar_t b[64]; DWORD sz = sizeof b;
    if (RegGetValueW(HKEY_CURRENT_USER, RK, L"city", RRF_RT_REG_SZ, 0, b, &sz) == ERROR_SUCCESS) { std::lock_guard<std::mutex> lk(cm); s_city = b; }
}
void SaveSettings() {
    wrI(L"theme", s_theme); wrI(L"dark", s_dark); wrI(L"wx", s_wx); wrI(L"launch", s_launch); wrI(L"art", s_art); wrI(L"hud", s_hud); wrI(L"lite", s_lite);
    std::lock_guard<std::mutex> lk(cm);
    RegSetKeyValueW(HKEY_CURRENT_USER, RK, L"city", REG_SZ, s_city.c_str(), (DWORD)((s_city.size() + 1) * 2));
}

// ---------- окно ----------
enum { ID_APPLE = 101, ID_MY, ID_DARK, ID_WX, ID_LAUNCH, ID_ART, ID_HUD, ID_LITE, ID_CITY };
static HWND hs = nullptr;
static int P(float v) { return (int)(v * g_S + .5f); }
static HWND mk(const wchar_t* cls, const wchar_t* t, DWORD st, float x, float y, float w, float h, int id, DWORD ex = 0) {
    return CreateWindowExW(ex, cls, t, WS_CHILD | WS_VISIBLE | st, P(x), P(y), P(w), P(h), hs, (HMENU)(INT_PTR)id, GetModuleHandle(0), 0);
}
static void readSet() {
    if (!hs) return;
    s_theme = IsDlgButtonChecked(hs, ID_MY) ? 1 : 0; s_dark = IsDlgButtonChecked(hs, ID_DARK) ? 1 : 0;
    s_wx = IsDlgButtonChecked(hs, ID_WX) ? 1 : 0; s_launch = IsDlgButtonChecked(hs, ID_LAUNCH) ? 1 : 0;
    s_art = IsDlgButtonChecked(hs, ID_ART) ? 1 : 0; s_hud = IsDlgButtonChecked(hs, ID_HUD) ? 1 : 0;
    s_lite = IsDlgButtonChecked(hs, ID_LITE) ? 1 : 0;
    wchar_t b[64] = { 0 }; GetDlgItemTextW(hs, ID_CITY, b, 64);
    { std::lock_guard<std::mutex> lk(cm); if (s_city != b && b[0]) { s_city = b; g_wxReset = true; } }
    if (s_wx) g_wxReset = true;
    g_themeDirty = true; SaveSettings();
}
static LRESULT CALLBACK SProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) readSet();
        else if (LOWORD(w) == ID_CITY) { if (HIWORD(w) == EN_KILLFOCUS) readSet(); }
        else if (HIWORD(w) == BN_CLICKED) readSet();
        return 0;
    case WM_TIMER: { RECT r{ P(20), P(104), P(340), P(136) }; InvalidateRect(h, &r, FALSE); return 0; }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); int idx[5] = { 2, 5, 0, 1, 3 };
        for (int i = 0; i < 5; i++) {
            HBRUSH b = CreateSolidBrush(RGB((int)thTgt[idx[i]][0], (int)thTgt[idx[i]][1], (int)thTgt[idx[i]][2]));
            RECT r{ P(24 + i * 62), P(110), P(24 + i * 62 + 54), P(132) }; FillRect(dc, &r, b); FrameRect(dc, &r, (HBRUSH)GetStockObject(GRAY_BRUSH)); DeleteObject(b);
        }
        EndPaint(h, &ps); return 0; }
    case WM_CLOSE: readSet(); ShowWindow(h, SW_HIDE); return 0;
    }
    return DefWindowProc(h, m, w, l);
}
void OpenSettings() {
    if (!hs) {
        WNDCLASSW wc{}; wc.lpfnWndProc = SProc; wc.hInstance = GetModuleHandle(0); wc.lpszClassName = L"WinIslandSet";
        wc.hCursor = LoadCursor(0, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); RegisterClassW(&wc);
        RECT r{ 0, 0, P(360), P(364) }; DWORD st = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU; AdjustWindowRect(&r, st, FALSE);
        int ww = r.right - r.left, wh = r.bottom - r.top;
        hs = CreateWindowExW(0, L"WinIslandSet", L"Dynamic Island — Настройки", st, (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh, 0, 0, GetModuleHandle(0), 0);
        mk(L"BUTTON", L"Темы", BS_GROUPBOX, 10, 8, 340, 136, 0);
        mk(L"BUTTON", L"Apple (чёрная классика)", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 24, 30, 300, 22, ID_APPLE);
        mk(L"BUTTON", L"Material You (цвета из обоев, как matugen)", BS_AUTORADIOBUTTON | WS_TABSTOP, 24, 54, 310, 22, ID_MY);
        mk(L"BUTTON", L"Тёмная схема Material You", BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 24, 80, 300, 22, ID_DARK);
        mk(L"BUTTON", L"Функции", BS_GROUPBOX, 10, 152, 340, 204, 0);
        mk(L"BUTTON", L"Погода и предупреждение о дожде", BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 24, 174, 310, 22, ID_WX);
        mk(L"BUTTON", L"Быстрые приложения", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 198, 310, 22, ID_LAUNCH);
        mk(L"BUTTON", L"Обложка альбома", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 222, 310, 22, ID_ART);
        mk(L"BUTTON", L"Всплывашки громкости и батареи", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 246, 310, 22, ID_HUD);
        mk(L"BUTTON", L"Лёгкий режим (без теней и свечения)", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 270, 310, 22, ID_LITE);
        mk(L"STATIC", L"Город для погоды:", SS_LEFT, 24, 312, 110, 20, 0);
        mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 140, 308, 190, 24, ID_CITY, WS_EX_CLIENTEDGE);
        NONCLIENTMETRICSW nm{ sizeof nm }; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof nm, &nm, 0);
        static HFONT f = CreateFontIndirectW(&nm.lfMessageFont);
        EnumChildWindows(hs, [](HWND c, LPARAM l) -> BOOL { SendMessage(c, WM_SETFONT, (WPARAM)l, TRUE); return TRUE; }, (LPARAM)f);
        CheckRadioButton(hs, ID_APPLE, ID_MY, s_theme ? ID_MY : ID_APPLE);
        auto ck = [](int id, int v) { CheckDlgButton(hs, id, v ? BST_CHECKED : BST_UNCHECKED); };
        ck(ID_DARK, s_dark); ck(ID_WX, s_wx); ck(ID_LAUNCH, s_launch); ck(ID_ART, s_art); ck(ID_HUD, s_hud); ck(ID_LITE, s_lite);
        SetDlgItemTextW(hs, ID_CITY, GetCity().c_str());
        SetTimer(hs, 2, 500, 0);
    }
    ShowWindow(hs, SW_SHOW); SetForegroundWindow(hs);
}
bool SettingsDialogMsg(MSG* m) { return hs && IsWindowVisible(hs) && IsDialogMessage(hs, m); }
