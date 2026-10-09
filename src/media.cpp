#include "base.h"
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Media.Control.h>
#include "media.h"
namespace MC = winrt::Windows::Media::Control;
namespace SS = winrt::Windows::Storage::Streams;

static Media g_m; static std::mutex mx;
static std::atomic<int> g_cmd{ 0 }; static std::atomic<double> g_seek{ 0 };
static Bitmap* artPend = nullptr; static bool artDirty = false;

Media MediaGet() { std::lock_guard<std::mutex> lk(mx); return g_m; }
void MediaSend(int cmd, double arg) {
    {   // оптимистичное обновление, чтобы UI реагировал мгновенно
        std::lock_guard<std::mutex> lk(mx); ULONGLONG now = GetTickCount64();
        if (cmd == 1) { g_m.pos += g_m.playing ? (now - g_m.t) / 1000.0 : 0; g_m.t = now; g_m.playing = !g_m.playing; }
        if (cmd == 4) { g_m.pos = arg; g_m.t = now; }
    }
    g_seek = arg; g_cmd = cmd;
}
bool MediaTakeArt(Bitmap*& art) {
    std::lock_guard<std::mutex> lk(mx);
    if (!artDirty) return false;
    delete art; art = artPend; artPend = nullptr; artDirty = false; return true;
}
static void pushArt(Bitmap* nb) { std::lock_guard<std::mutex> lk(mx); delete artPend; artPend = nb; artDirty = true; }

static Bitmap* mkArt(std::vector<BYTE>& v) {   // обложка -> квадрат 256x256
    if (v.empty()) return nullptr;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, v.size()); if (!hg) return nullptr;
    void* p = GlobalLock(hg); memcpy(p, v.data(), v.size()); GlobalUnlock(hg);
    IStream* st = nullptr; if (FAILED(CreateStreamOnHGlobal(hg, TRUE, &st))) { GlobalFree(hg); return nullptr; }
    Bitmap* src = Bitmap::FromStream(st); Bitmap* out = nullptr;
    if (src && src->GetLastStatus() == Ok) {
        out = new Bitmap(256, 256, PixelFormat32bppPARGB); Graphics gg(out);
        gg.SetInterpolationMode(InterpolationModeHighQualityBicubic); gg.SetPixelOffsetMode(PixelOffsetModeHalf);
        UINT w = src->GetWidth(), h = src->GetHeight(), s = min(w, h);
        gg.DrawImage(src, RectF(0, 0, 256, 256), (REAL)((w - s) / 2), (REAL)((h - s) / 2), (REAL)s, (REAL)s, UnitPixel);
    }
    delete src; st->Release(); return out;
}
static std::vector<BYTE> readThumb(MC::GlobalSystemMediaTransportControlsSessionMediaProperties const& p) {
    auto ref = p.Thumbnail(); if (!ref) return {};
    auto st = ref.OpenReadAsync().get(); uint32_t n = (uint32_t)st.Size(); if (!n || n > 8000000) return {};
    SS::DataReader dr(st.GetInputStreamAt(0)); dr.LoadAsync(n).get();
    std::vector<BYTE> v(n); dr.ReadBytes(v); return v;
}

void MediaThread() {
    winrt::init_apartment();
    MC::GlobalSystemMediaTransportControlsSessionManager mgr{ nullptr };
    try { mgr = MC::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get(); } catch (...) {}
    std::wstring lastKey;
    while (g_run && mgr) {
        try {
            auto s = mgr.GetCurrentSession(); int c = g_cmd.exchange(0);
            if (s) {
                if (c == 1) s.TryTogglePlayPauseAsync().get();
                if (c == 2) s.TrySkipPreviousAsync().get();
                if (c == 3) s.TrySkipNextAsync().get();
                if (c == 4) s.TryChangePlaybackPositionAsync((int64_t)(g_seek.load() * 1e7)).get();
                auto p = s.TryGetMediaPropertiesAsync().get(); auto pb = s.GetPlaybackInfo(); auto tl = s.GetTimelineProperties();
                std::wstring title = p.Title().c_str(), artist = p.Artist().c_str(), key = title + L"|" + artist;
                if (key != lastKey) { lastKey = key; std::vector<BYTE> v; try { v = readThumb(p); } catch (...) {} pushArt(mkArt(v)); }
                std::lock_guard<std::mutex> lk(mx);
                g_m.has = true; g_m.title = title; g_m.artist = artist;
                g_m.playing = pb.PlaybackStatus() == MC::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                g_m.pos = std::chrono::duration_cast<std::chrono::milliseconds>(tl.Position()).count() / 1000.0;
                g_m.dur = std::chrono::duration_cast<std::chrono::milliseconds>(tl.EndTime()).count() / 1000.0;
                g_m.t = GetTickCount64();
            } else {
                if (!lastKey.empty()) { lastKey.clear(); pushArt(nullptr); }
                std::lock_guard<std::mutex> lk(mx); g_m.has = false; g_m.playing = false;
            }
        } catch (...) {}
        for (int i = 0; i < 10 && g_run && !g_cmd; i++) Sleep(50);
    }
}
