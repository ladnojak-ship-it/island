#include "fonts.h"
#include <cwctype>

static PrivateFontCollection* pfc = nullptr;
static FontFamily* fam[2] = { nullptr, nullptr };
static std::wstring famName[2];
// по порядку: берём первый найденный
static const wchar_t* L_APPLE[] = { L"SF Pro Text", L"SF Pro Display", L"SF Pro", L"Inter", L"Segoe UI Variable Text", L"Segoe UI", nullptr };
static const wchar_t* L_MATERIAL[] = { L"Google Sans", L"Google Sans Text", L"Google Sans Flex", L"Product Sans", L"Roboto Flex", L"Roboto", L"Segoe UI Variable Text", L"Segoe UI", nullptr };

static FontFamily* Find(const wchar_t* name) {
    if (pfc && pfc->GetFamilyCount() > 0) {
        FontFamily* f = new FontFamily(name, pfc);
        if (f->GetLastStatus() == Ok) return f;
        delete f;
    }
    FontFamily* f = new FontFamily(name, nullptr);
    if (f->GetLastStatus() == Ok) return f;
    delete f; return nullptr;
}

void FontsInit() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
    std::wstring dir = exe; size_t sl = dir.find_last_of(L"\\/"); dir = (sl == std::wstring::npos ? L"" : dir.substr(0, sl + 1)) + L"fonts\\";
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"*.*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        pfc = new PrivateFontCollection();
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring n = fd.cFileName; size_t d = n.find_last_of(L'.'); if (d == std::wstring::npos) continue;
            std::wstring e = n.substr(d); for (auto& c : e) c = (wchar_t)towlower(c);
            if (e == L".ttf" || e == L".otf" || e == L".ttc") {
                pfc->AddFontFile((dir + n).c_str());
                AddFontResourceExW((dir + n).c_str(), FR_PRIVATE, 0);      // чтобы имя семейства видели и обычные GDI-контролы
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    for (int t = 0; t < 2; t++) {
        const wchar_t** lst = t ? L_MATERIAL : L_APPLE;
        for (int i = 0; lst[i] && !fam[t]; i++) fam[t] = Find(lst[i]);
        if (!fam[t]) fam[t] = new FontFamily(L"Segoe UI", nullptr);
        WCHAR nm[LF_FACESIZE] = { 0 }; fam[t]->GetFamilyName(nm); famName[t] = nm;
    }
}

Font* MkFont(int theme, float px, bool bold) {
    FontFamily* f = fam[theme ? 1 : 0];
    int st = (bold && f->IsStyleAvailable(FontStyleBold)) ? FontStyleBold : FontStyleRegular;
    Font* ft = new Font(f, px, st, UnitPixel);
    if (ft->GetLastStatus() != Ok) { delete ft; ft = new Font(L"Segoe UI", px, FontStyleRegular, UnitPixel); }
    return ft;
}
std::wstring FontFamilyName(int theme) { return famName[theme ? 1 : 0]; }
