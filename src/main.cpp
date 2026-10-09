// main.cpp — точка входа: окно, трей, меню, цикл сообщений с синхронизацией по vsync
#include "base.h"
#include <timeapi.h>
#include <dwmapi.h>
#include "gdi.h"
#include "island.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "media.h"
#include "weather.h"
#include "sysinfo.h"
#include "fonts.h"
#include "gfx.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::atomic<bool> g_run{ true };
float g_S = 1;
float g_dpi = 1;
static HWND hw;

// иконка в трее: белая «пилюля» с тёмной точкой (рисуем сами, без .ico)
static HICON MakeTrayIcon() {
    Bitmap b(32, 32, PixelFormat32bppARGB); HICON h = nullptr;
    { Graphics g(&b); g.SetSmoothingMode(SmoothingModeHighQuality); g.Clear(Color(0, 0, 0, 0));
      GraphicsPath p; RR(p, RectF(1, 9, 30, 14), 7); SolidBrush w(Color(255, 245, 245, 245)); g.FillPath(&w, &p);
      SolidBrush d(Color(255, 30, 30, 34)); g.FillEllipse(&d, 21.f, 12.f, 8.f, 8.f); }
    b.GetHICON(&h); return h ? h : LoadIcon(0, IDI_APPLICATION);
}

static int ShowMenu() {
    HMENU mn = CreatePopupMenu(); AppendMenuW(mn, MF_STRING, 1, L"Настройки"); AppendMenuW(mn, MF_STRING, 2, L"Выход");
    POINT p; GetCursorPos(&p); IslandMenuOpen(true); SetForegroundWindow(hw);
    int c = TrackPopupMenu(mn, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hw, 0);
    IslandMenuOpen(false); DestroyMenu(mn); return c;
}
static void MenuAct() { int c = ShowMenu(); if (c == 1) OpenSettings(); else if (c == 2) DestroyWindow(hw); }

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDOWN: IslandMouseDown(); return 0;
    case WM_MOUSEMOVE: IslandMouseMove(); return 0;
    case WM_LBUTTONUP: IslandMouseUp(); return 0;
    case WM_MOUSEWHEEL: IslandWheel(GET_WHEEL_DELTA_WPARAM(w)); return 0;
    case WM_RBUTTONUP: MenuAct(); return 0;
    case WM_APP + 1:                       // иконка в трее
        if (LOWORD(l) == WM_RBUTTONUP) MenuAct(); else if (LOWORD(l) == WM_LBUTTONDBLCLK) OpenSettings();
        return 0;
    case WM_COPYDATA: {                    // island.exe --notify "текст"
        COPYDATASTRUCT* cd = (COPYDATASTRUCT*)l;
        if (cd && cd->dwData == 1 && cd->lpData) {
            std::wstring s((wchar_t*)cd->lpData, cd->cbData / sizeof(wchar_t));
            while (!s.empty() && s.back() == 0) s.pop_back();
            IslandNotify(s);
        }
        return TRUE; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    if (__argc >= 3 && !wcscmp(__wargv[1], L"--notify")) {     // клиентский режим: отправить уведомление и выйти
        HWND t = FindWindowW(L"WinIsland", 0);
        if (t) { std::wstring s = __wargv[2]; COPYDATASTRUCT cd{ 1, (DWORD)((s.size() + 1) * 2), (void*)s.c_str() }; SendMessage(t, WM_COPYDATA, 0, (LPARAM)&cd); }
        return 0;
    }
    HANDLE mtx = CreateMutexW(0, TRUE, L"WinIslandSingleInstance"); if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    SetProcessDPIAware(); g_dpi = GetDpiForSystem() / 96.f;
    CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    GdiplusStartupInput gi; ULONG_PTR tok; GdiplusStartup(&tok, &gi, 0);
    LoadSettings(); g_S = g_dpi * s_scale.load() / 100.f; FontsInit(); ThemeInit(); VolInit(); SysUpdate();

    WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = L"WinIsland"; wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClassW(&wc);
    hw = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"WinIsland", L"Island", WS_POPUP, 0, 0, 1, 1, 0, 0, hi, 0);
    if (!RenderInit(hw)) return 1;
    IslandInit(hw);
    ShowWindow(hw, SW_SHOWNOACTIVATE);

    NOTIFYICONDATAW nid{ sizeof nid }; nid.hWnd = hw; nid.uID = 1; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP + 1; nid.hIcon = MakeTrayIcon(); wcscpy_s(nid.szTip, L"Dynamic Island");
    Shell_NotifyIconW(NIM_ADD, &nid);

    std::thread tMedia(MediaThread), tWall(WallpaperThread), tWx(WeatherThread);
    timeBeginPeriod(1);
    bool quit = false;
    while (!quit) {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { quit = true; break; }
            if (SettingsDialogMsg(&msg)) continue;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (quit) break;
        int wait = IslandTick();
        if (wait <= 0) { if (FAILED(DwmFlush())) Sleep(8); }                  // анимация: ждём следующий кадр монитора
        else MsgWaitForMultipleObjects(0, 0, FALSE, wait, QS_ALLINPUT);      // покой: почти 0% CPU
    }
    timeEndPeriod(1);
    g_run = false; tMedia.join(); tWall.join(); tWx.join();
    Shell_NotifyIconW(NIM_DELETE, &nid); ReleaseMutex(mtx);
    return 0;
}
