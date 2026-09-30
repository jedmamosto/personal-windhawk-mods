#pragma once

#ifndef MEDIA_SMTC_ENGINE_HPP
#define MEDIA_SMTC_ENGINE_HPP

// ============================================================================
// Dynamic Island for Windows - Media & SMTC Subsystem Engine
// Architecture:
// 1. WinRT System Media Transport Controls (SMTC) Session Manager & Listeners
//    (PlaybackInfoChanged, MediaPropertiesChanged, TimelinePropertiesChanged)
// 2. Metadata Extraction, Thumbnail Decoding & Smart Settle Caching
// 3. VLC Fallback Window Title Scanning for sessions missing metadata
// 4. WASAPI Audio Loopback Capture & 48-Sample Waveform Ring Buffer with EMA Smoothing
// 5. Remote Transport Control Dispatch (Play, Pause, Prev, Next, Seek, Open App)
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <wincodec.h>

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <optional>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include "island_common.hpp"
#include "icon_process_engine.hpp"
#include "palette_color_engine.hpp"

// External state symbols defined by the mod core
extern HANDLE g_stopEvent;
extern HWND g_hwnd;
extern std::atomic<bool> g_layoutDirty;
extern std::atomic<bool> g_audioCaptureNeeded;
extern std::atomic<bool> g_autoHiddenParked;
extern std::atomic<double> g_lastNudgeTime;

// Forward declaration of nudge mechanism
void TriggerNudge();

namespace MediaEngine {

using Microsoft::WRL::ComPtr;
namespace winrt_media = winrt::Windows::Media::Control;

// Subtype GUID for IEEE Float audio formats in WASAPI loopback
inline constexpr GUID kSubTypeIeeeFloat = {
    0x00000003,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
};

// ----------------------------------------------------------------------------
// Local Nudge Trigger Helper
// ----------------------------------------------------------------------------
inline void TriggerIslandNudge() {
    const bool wasParked = g_autoHiddenParked.exchange(false, std::memory_order_relaxed);
    const double now = NowSeconds();
    const double previous = g_lastNudgeTime.load();
    if (!wasParked && now - previous < 0.45) {
        return;
    }
    g_lastNudgeTime = now;
    HWND hwnd = g_hwnd;
    if (hwnd) {
        PostMessageW(hwnd, WM_APP_NEW_EVENT, 0, 0);
    }
}

inline bool DecodeImageBytesToPixels(const std::vector<uint8_t>& bytes, BitmapPixels* outPixels) {
    if (!outPixels || bytes.empty()) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!mem) {
        return false;
    }

    void* locked = GlobalLock(mem);
    memcpy(locked, bytes.data(), bytes.size());
    GlobalUnlock(mem);

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(mem, TRUE, &stream);
    if (FAILED(hr)) {
        GlobalFree(mem);
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                          &decoder);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return false;
    }

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0) {
        return false;
    }

    BitmapPixels pixels;
    pixels.width = width;
    pixels.height = height;
    pixels.bgra.resize(static_cast<size_t>(width) * height * 4);

    hr = converter->CopyPixels(nullptr, width * 4,
                               static_cast<UINT>(pixels.bgra.size()),
                               pixels.bgra.data());
    if (FAILED(hr)) {
        return false;
    }

    ExtractDominantColor(pixels);

    pixels.generation = ++g_artGenerationCounter;
    *outPixels = std::move(pixels);
    return true;
}

inline bool WindowLooksLikeMediaSource(HWND hwnd, const std::wstring& sourceLower) {
    if (!IsWindowVisible(hwnd) || hwnd == g_hwnd) {
        return false;
    }

    std::wstring image;
    if (!ProcessImageNameForWindow(hwnd, &image)) {
        return false;
    }

    const std::wstring base = ToLowerCopy(BaseNameFromPath(image));
    if (base.empty()) {
        return false;
    }

    return sourceLower.find(base) != std::wstring::npos ||
           (base.find(L"chrome") != std::wstring::npos && sourceLower.find(L"chrome") != std::wstring::npos) ||
           (base.find(L"msedge") != std::wstring::npos && sourceLower.find(L"edge") != std::wstring::npos) ||
           (base.find(L"vlc") != std::wstring::npos && sourceLower.find(L"vlc") != std::wstring::npos);
}

inline BOOL CALLBACK FindMediaSourceWindowProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<std::pair<const std::wstring*, HWND*>*>(lParam);
    if (WindowLooksLikeMediaSource(hwnd, *data->first)) {
        *data->second = hwnd;
        return FALSE;
    }
    return TRUE;
}

inline std::wstring FindBrowserMediaSiteBadge(const std::wstring& sourceAppUserModelId) {
    const std::wstring sourceLower = ToLowerCopy(sourceAppUserModelId);
    HWND found = nullptr;
    std::pair<const std::wstring*, HWND*> data{&sourceLower, &found};
    EnumWindows(FindMediaSourceWindowProc, reinterpret_cast<LPARAM>(&data));
    if (found) {
        wchar_t title[192] = {};
        GetWindowTextW(found, title, ARRAYSIZE(title));
        return SiteBadgeFromTitle(title);
    }
    return L"WEB";
}

inline BitmapPixels FindMediaSourceIcon(const std::wstring& sourceAppUserModelId) {
    BitmapPixels pixels;
    const std::wstring sourceLower = ToLowerCopy(sourceAppUserModelId);
    HWND found = nullptr;
    std::pair<const std::wstring*, HWND*> data{&sourceLower, &found};
    EnumWindows(FindMediaSourceWindowProc, reinterpret_cast<LPARAM>(&data));
    if (found) {
        pixels = GetWindowIconPixels(found, 32);
    }
    return pixels;
}

// ----------------------------------------------------------------------------
// Media Auto-Expand Exclusion (#62)
// ----------------------------------------------------------------------------
inline bool MediaExpandBlocked(const MediaSnapshot& media) {
    {
        std::lock_guard lock(g_settingsMutex);
        if (g_settings.mediaExpandBlocklist.empty()) {
            return false;
        }
    }

    const std::wstring haystack = ToLowerCopy(
        media.sourceName + L"\n" + media.sourceAppUserModelId + L"\n" + media.title);

    std::lock_guard lock(g_settingsMutex);
    for (const std::wstring& needle : g_settings.mediaExpandBlocklist) {
        if (!needle.empty() && haystack.find(needle) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// WinRT Thumbnail Stream Reader
// ----------------------------------------------------------------------------
inline std::vector<uint8_t> ReadWinRtStreamBytes(
    const winrt::Windows::Storage::Streams::IRandomAccessStreamReference& reference) {
    std::vector<uint8_t> bytes;
    if (!reference) {
        return bytes;
    }

    auto stream = reference.OpenReadAsync().get();
    if (!stream) {
        return bytes;
    }

    const uint64_t size64 = stream.Size();
    if (size64 == 0 || size64 > 8 * 1024 * 1024) {
        return bytes;
    }

    const uint32_t size = static_cast<uint32_t>(size64);
    winrt::Windows::Storage::Streams::DataReader reader(stream.GetInputStreamAt(0));
    reader.LoadAsync(size).get();
    bytes.resize(size);
    reader.ReadBytes(winrt::array_view<uint8_t>(bytes.data(), bytes.data() + bytes.size()));
    return bytes;
}

// ----------------------------------------------------------------------------
// VLC Fallback Window Title Parser
// ----------------------------------------------------------------------------
// VLC often exposes an SMTC session with no Title metadata at all.
// The marker must still be at the END of the caption, matching real VLC windows.
inline bool TryParseVlcWindowTitle(std::wstring& outTitle) {
    const std::wstring marker = L"VLC media player";
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExW(nullptr, hwnd, nullptr, nullptr)) != nullptr) {
        if (!IsWindowVisible(hwnd)) {
            continue;
        }

        wchar_t windowTitle[512];
        if (GetWindowTextW(hwnd, windowTitle, ARRAYSIZE(windowTitle)) <= 0) {
            continue;
        }

        std::wstring caption(windowTitle);
        while (!caption.empty() && caption.back() == L' ') {
            caption.pop_back();
        }
        if (caption.size() <= marker.size() ||
            caption.compare(caption.size() - marker.size(),
                            marker.size(), marker) != 0) {
            continue;
        }

        std::wstring image;
        if (!ProcessImageNameForWindow(hwnd, &image) ||
            ToLowerCopy(BaseNameFromPath(image)).find(L"vlc") == std::wstring::npos) {
            continue;
        }

        std::wstring candidate = caption.substr(0, caption.size() - marker.size());
        while (!candidate.empty() &&
               (candidate.back() == L' ' || candidate.back() == L'-')) {
            candidate.pop_back();
        }
        if (!candidate.empty()) {
            outTitle = candidate;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// SMTC Session Selection
// ----------------------------------------------------------------------------
inline winrt_media::GlobalSystemMediaTransportControlsSession SelectActiveMediaSession(
    winrt_media::GlobalSystemMediaTransportControlsSessionManager const& manager) {
    using PlaybackStatus = winrt_media::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

    auto sessionIsPlaying = [](auto const& candidate) -> bool {
        if (!candidate) {
            return false;
        }
        try {
            return candidate.GetPlaybackInfo().PlaybackStatus() == PlaybackStatus::Playing;
        } catch (...) {
            return false;
        }
    };

    winrt_media::GlobalSystemMediaTransportControlsSession current = nullptr;
    try {
        current = manager.GetCurrentSession();
    } catch (...) {}

    if (sessionIsPlaying(current)) {
        return current;
    }

    try {
        for (auto const& candidate : manager.GetSessions()) {
            if (sessionIsPlaying(candidate)) {
                return candidate;
            }
        }
    } catch (...) {}

    return current;
}

inline winrt_media::GlobalSystemMediaTransportControlsSession ResolveMediaSession() {
    using Manager = winrt_media::GlobalSystemMediaTransportControlsSessionManager;
    auto manager = Manager::RequestAsync().get();
    if (!manager) {
        return nullptr;
    }

    std::wstring currentAumid;
    {
        std::lock_guard lock(g_stateMutex);
        currentAumid = g_state.media.sourceAppUserModelId;
    }

    if (!currentAumid.empty()) {
        for (auto const& s : manager.GetSessions()) {
            if (s.SourceAppUserModelId().c_str() == currentAumid) {
                return s;
            }
        }
    }
    return manager.GetCurrentSession();
}

// ----------------------------------------------------------------------------
// WinRT SMTC Event Subscriptions
// ----------------------------------------------------------------------------

class SmtcSessionWatcher {
public:
    SmtcSessionWatcher() = default;
    ~SmtcSessionWatcher() {
        Unsubscribe();
    }

    void Subscribe(winrt_media::GlobalSystemMediaTransportControlsSession const& session,
                   std::function<void()> onUpdate) {
        Unsubscribe();
        m_session = session;
        m_callback = std::move(onUpdate);
        if (!m_session) return;

        try {
            m_playbackToken = m_session.PlaybackInfoChanged(
                [this](auto const&, auto const&) {
                    if (m_callback) m_callback();
                });
            m_mediaPropertiesToken = m_session.MediaPropertiesChanged(
                [this](auto const&, auto const&) {
                    if (m_callback) m_callback();
                });
            m_timelineToken = m_session.TimelinePropertiesChanged(
                [this](auto const&, auto const&) {
                    if (m_callback) m_callback();
                });
        } catch (...) {}
    }

    void Unsubscribe() {
        if (m_session) {
            try {
                if (m_playbackToken) {
                    m_session.PlaybackInfoChanged(m_playbackToken);
                    m_playbackToken = {};
                }
                if (m_mediaPropertiesToken) {
                    m_session.MediaPropertiesChanged(m_mediaPropertiesToken);
                    m_mediaPropertiesToken = {};
                }
                if (m_timelineToken) {
                    m_session.TimelinePropertiesChanged(m_timelineToken);
                    m_timelineToken = {};
                }
            } catch (...) {}
            m_session = nullptr;
        }
    }

private:
    winrt_media::GlobalSystemMediaTransportControlsSession m_session{nullptr};
    std::function<void()> m_callback;
    winrt::event_token m_playbackToken{};
    winrt::event_token m_mediaPropertiesToken{};
    winrt::event_token m_timelineToken{};
};

class SmtcManagerWatcher {
public:
    SmtcManagerWatcher() = default;
    ~SmtcManagerWatcher() {
        Unsubscribe();
    }

    void Subscribe(winrt_media::GlobalSystemMediaTransportControlsSessionManager const& manager,
                   std::function<void()> onSessionChanged) {
        Unsubscribe();
        m_manager = manager;
        m_callback = std::move(onSessionChanged);
        if (!m_manager) return;

        try {
            m_currentSessionToken = m_manager.CurrentSessionChanged(
                [this](auto const&, auto const&) {
                    if (m_callback) m_callback();
                });
            m_sessionsChangedToken = m_manager.SessionsChanged(
                [this](auto const&, auto const&) {
                    if (m_callback) m_callback();
                });
        } catch (...) {}
    }

    void Unsubscribe() {
        if (m_manager) {
            try {
                if (m_currentSessionToken) {
                    m_manager.CurrentSessionChanged(m_currentSessionToken);
                    m_currentSessionToken = {};
                }
                if (m_sessionsChangedToken) {
                    m_manager.SessionsChanged(m_sessionsChangedToken);
                    m_sessionsChangedToken = {};
                }
            } catch (...) {}
            m_manager = nullptr;
        }
    }

private:
    winrt_media::GlobalSystemMediaTransportControlsSessionManager m_manager{nullptr};
    std::function<void()> m_callback;
    winrt::event_token m_currentSessionToken{};
    winrt::event_token m_sessionsChangedToken{};
};

// ----------------------------------------------------------------------------
// Core Snapshot Extraction & State Commit
// ----------------------------------------------------------------------------

inline bool UpdateMediaSnapshot(MediaSnapshot& next,
                                winrt_media::GlobalSystemMediaTransportControlsSession const& session) {
    if (!session) {
        return false;
    }

    using PlaybackStatus = winrt_media::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

    try {
        auto properties = session.TryGetMediaPropertiesAsync().get();
        auto playback = session.GetPlaybackInfo();
        auto timeline = session.GetTimelineProperties();

        next.available = true;
        next.playing = (playback.PlaybackStatus() == PlaybackStatus::Playing);
        next.title = properties.Title().c_str();
        next.artist = properties.Artist().c_str();
        next.albumTitle = properties.AlbumTitle().c_str();

        if (timeline) {
            int64_t np = timeline.Position().count();
            int64_t ne = timeline.EndTime().count();
            bool npP = (playback.PlaybackStatus() == PlaybackStatus::Playing);

            std::lock_guard lock(g_stateMutex);
            if (np != g_state.media.positionTicks ||
                ne != g_state.media.endTicks ||
                npP != g_state.media.playing) {
                next.positionTicks = np;
                next.endTicks = ne;
                next.lastUpdatedTicks = GetTickCount64();
            } else {
                next.positionTicks = g_state.media.positionTicks;
                next.endTicks = g_state.media.endTicks;
                next.lastUpdatedTicks = g_state.media.lastUpdatedTicks;
            }
        }

        next.sourceAppUserModelId = session.SourceAppUserModelId().c_str();
        next.sourceName = FriendlyMediaSourceName(next.sourceAppUserModelId);

        // VLC fallback: parse window title if SMTC metadata is missing
        if (next.title.empty() && next.sourceName == L"VLC") {
            TryParseVlcWindowTitle(next.title);
        }

        std::wstring prevSourceAppUserModelId;
        bool hasPrevIcon = false;
        BitmapPixels prevIcon;
        uint64_t prevIconGeneration = 0;
        std::wstring prevBadge;

        std::wstring prevTitle;
        std::wstring prevArtist;
        BitmapPixels prevArt;
        uint64_t prevArtGeneration = 0;
        double prevArtChangedAt = 0.0;
        double prevTitleChangedAt = 0.0;
        bool prevPlaying = false;

        {
            std::lock_guard lock(g_stateMutex);
            prevSourceAppUserModelId = g_state.media.sourceAppUserModelId;
            hasPrevIcon = !g_state.media.sourceIcon.bgra.empty();
            prevIcon = g_state.media.sourceIcon;
            prevIconGeneration = g_state.media.sourceIconGeneration;
            prevBadge = g_state.media.sourceBadge;

            prevTitle = g_state.media.title;
            prevArtist = g_state.media.artist;
            prevArt = g_state.media.art;
            prevArtGeneration = g_state.media.artGeneration;
            prevArtChangedAt = g_state.media.artChangedAt;
            prevTitleChangedAt = g_state.media.titleChangedAt;
            prevPlaying = g_state.media.playing;
        }

        if (next.sourceAppUserModelId == prevSourceAppUserModelId) {
            next.sourceBadge = prevBadge;
            next.sourceIcon = prevIcon;
            next.sourceIconGeneration = prevIconGeneration;

            if (!hasPrevIcon) {
                next.sourceIcon = FindMediaSourceIcon(next.sourceAppUserModelId);
                next.sourceIconGeneration = next.sourceIcon.generation;
            }
        } else {
            next.sourceBadge = MediaSourceBadge(next.sourceName);
            if (IsBrowserMediaSource(next.sourceAppUserModelId)) {
                next.sourceBadge = FindBrowserMediaSiteBadge(next.sourceAppUserModelId);
            }
            next.sourceIcon = FindMediaSourceIcon(next.sourceAppUserModelId);
            next.sourceIconGeneration = next.sourceIcon.generation;
        }

        const bool metadataChanged = (next.title != prevTitle || next.artist != prevArtist || (!prevPlaying && next.playing));
        next.titleChangedAt = (metadataChanged && !next.title.empty() && next.playing) ? NowSeconds() : prevTitleChangedAt;
        constexpr double kArtSettleSeconds = 3.0;
        const bool artSettled = !metadataChanged &&
            (NowSeconds() - prevTitleChangedAt) > kArtSettleSeconds;

        if (artSettled && !prevArt.bgra.empty()) {
            next.art = prevArt;
            next.artGeneration = prevArtGeneration;
            next.artChangedAt = prevArtChangedAt;
        } else if (auto thumbnail = properties.Thumbnail()) {
            std::vector<uint8_t> bytes = ReadWinRtStreamBytes(thumbnail);
            if (!bytes.empty()) {
                BitmapPixels decoded;
                if (DecodeImageBytesToPixels(bytes, &decoded)) {
                    next.art = std::move(decoded);
                    next.artGeneration = next.art.generation;
                    next.artChangedAt = NowSeconds();
                }
            }
        }

        if (next.art.bgra.empty() && !prevArt.bgra.empty() && !metadataChanged) {
            next.art = prevArt;
            next.artGeneration = prevArtGeneration;
            next.artChangedAt = prevArtChangedAt;
        }

        return true;
    } catch (...) {
        return false;
    }
}

inline bool CommitMediaSnapshot(MediaSnapshot&& next) {
    bool trackJustChanged = false;
    {
        std::lock_guard lock(g_stateMutex);
        const bool isDifferentTrack = (!next.title.empty() && next.playing) &&
            (next.title != g_state.media.title || next.artist != g_state.media.artist || (!g_state.media.playing && next.playing));

        if (isDifferentTrack) {
            next.titleChangedAt = NowSeconds();
            trackJustChanged = true;
            g_idleTab = 0;
        }

        if (!g_state.media.art.bgra.empty() &&
            next.title == g_state.media.title && next.artist == g_state.media.artist &&
            next.positionTicks >= g_state.media.positionTicks) {
            next.art = g_state.media.art;
            next.artGeneration = g_state.media.artGeneration;
            next.artChangedAt = g_state.media.artChangedAt;
        }
        if (next.sourceIcon.bgra.empty() &&
            next.sourceAppUserModelId == g_state.media.sourceAppUserModelId) {
            next.sourceIcon = g_state.media.sourceIcon;
            next.sourceIconGeneration = g_state.media.sourceIconGeneration;
        }

        g_state.media = std::move(next);
    }

    if (trackJustChanged) {
        g_layoutDirty = true;
        if (g_settings.mediaAutoExpand) {
            TriggerNudge();
        }
    }
    return trackJustChanged;
}

inline bool UpdateMediaSnapshot() {
    using Manager = winrt_media::GlobalSystemMediaTransportControlsSessionManager;
    try {
        auto manager = Manager::RequestAsync().get();
        if (manager) {
            auto session = SelectActiveMediaSession(manager);
            if (session) {
                MediaSnapshot next;
                if (UpdateMediaSnapshot(next, session)) {
                    CommitMediaSnapshot(std::move(next));
                    return true;
                }
            }
        }
    } catch (...) {}
    return false;
}

// ----------------------------------------------------------------------------
// Remote Transport Controls & Seeking
// ----------------------------------------------------------------------------

inline void SendMediaTransportCommand(int cmd) {
    std::thread([cmd]() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        bool sessionAttempted = false;
        bool reportedSuccess = false;
        try {
            auto session = ResolveMediaSession();
            if (session) {
                sessionAttempted = true;
                if (cmd == 0) {
                    reportedSuccess = session.TrySkipPreviousAsync().get();
                } else if (cmd == 1) {
                    reportedSuccess = session.TryTogglePlayPauseAsync().get();
                } else if (cmd == 2) {
                    reportedSuccess = session.TrySkipNextAsync().get();
                }
            }
        } catch (...) {
            sessionAttempted = false;
        }

        if (sessionAttempted) {
            if (!reportedSuccess) {
                Wh_Log(L"Media transport command %d was refused by the session.", cmd);
            }
            return;
        }

        BYTE vk = 0;
        if (cmd == 0) {
            vk = VK_MEDIA_PREV_TRACK;
        } else if (cmd == 1) {
            vk = VK_MEDIA_PLAY_PAUSE;
        } else if (cmd == 2) {
            vk = VK_MEDIA_NEXT_TRACK;
        }
        if (vk != 0) {
            keybd_event(vk, 0, KEYEVENTF_EXTENDEDKEY, 0);
            keybd_event(vk, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        }
    }).detach();
}

inline void SeekMediaToTicks(int64_t targetTicks) {
    std::thread([targetTicks]() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try {
            using Manager = winrt_media::GlobalSystemMediaTransportControlsSessionManager;
            auto manager = Manager::RequestAsync().get();
            if (manager) {
                auto sessions = manager.GetSessions();
                std::wstring currentAumid;
                {
                    std::lock_guard lock(g_stateMutex);
                    currentAumid = g_state.media.sourceAppUserModelId;
                }
                winrt_media::GlobalSystemMediaTransportControlsSession session = nullptr;
                for (auto const& s : sessions) {
                    if (s.SourceAppUserModelId().c_str() == currentAumid) {
                        session = s;
                        break;
                    }
                }
                if (!session) session = manager.GetCurrentSession();

                if (session) {
                    session.TryChangePlaybackPositionAsync(targetTicks).get();
                }
            }
        } catch (...) {}
    }).detach();
}

// ----------------------------------------------------------------------------
// Open Relevant Media App
// ----------------------------------------------------------------------------

struct WindowSearch {
    std::wstring targetTitle;
    std::wstring targetApp;
    HWND foundHwnd = nullptr;
    HWND fallbackHwnd = nullptr;
};

inline BOOL CALLBACK EnumWindowsForAppProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    auto* search = reinterpret_cast<WindowSearch*>(lParam);

    wchar_t title[512];
    if (GetWindowTextW(hwnd, title, ARRAYSIZE(title)) > 0) {
        std::wstring wTitle(title);
        auto it = std::search(
            wTitle.begin(), wTitle.end(),
            search->targetTitle.begin(), search->targetTitle.end(),
            [](wchar_t ch1, wchar_t ch2) { return towlower(ch1) == towlower(ch2); }
        );

        if (it != wTitle.end()) {
            search->foundHwnd = hwnd;
            return FALSE;
        }
    }

    if (!search->fallbackHwnd && !search->targetApp.empty()) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0) {
            std::wstring exePath;
            if (ProcessImageNameForPid(pid, &exePath)) {
                std::wstring targetLower = ToLowerCopy(search->targetApp);
                std::wstring exeLower = ToLowerCopy(exePath);

                if (targetLower.size() >= 2 && targetLower.front() == L'"' && targetLower.back() == L'"') {
                    targetLower = targetLower.substr(1, targetLower.size() - 2);
                }

                if (exeLower == targetLower) {
                    search->fallbackHwnd = hwnd;
                } else {
                    std::wstring exeName = exeLower;
                    size_t slashPos = exeName.find_last_of(L"\\/");
                    if (slashPos != std::wstring::npos) {
                        exeName = exeName.substr(slashPos + 1);
                    }
                    if (exeName == targetLower || exeName == targetLower + L".exe") {
                        search->fallbackHwnd = hwnd;
                    }
                }
            }
        }
    }

    return TRUE;
}

inline void OpenRelevantApp() {
    std::wstring title;
    std::wstring app;
    {
        std::lock_guard lock(g_stateMutex);
        title = g_state.media.title;
        app = g_state.media.sourceAppUserModelId;
    }

    if (!title.empty() || !app.empty()) {
        WindowSearch search;
        search.targetTitle = title;
        search.targetApp = app;
        EnumWindows(EnumWindowsForAppProc, reinterpret_cast<LPARAM>(&search));

        HWND hwndToFocus = search.foundHwnd ? search.foundHwnd : search.fallbackHwnd;

        if (hwndToFocus) {
            if (IsIconic(hwndToFocus)) {
                ShowWindow(hwndToFocus, SW_RESTORE);
            }
            SetForegroundWindow(hwndToFocus);
            return;
        }
    }

    if (!app.empty()) {
        std::wstring executePath = app;

        if (executePath.size() >= 2 && executePath.front() == L'"' && executePath.back() == L'"') {
            executePath = executePath.substr(1, executePath.size() - 2);
        }

        bool isFilePath = (executePath.find(L":\\") != std::wstring::npos ||
                           (executePath.size() >= 4 && executePath.substr(executePath.size() - 4) == L".exe"));

        if (isFilePath) {
            if (GetFileAttributesW(executePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                size_t x86Pos = executePath.find(L" (x86)");
                if (x86Pos != std::wstring::npos) {
                    std::wstring altPath = executePath;
                    altPath.erase(x86Pos, 6);
                    if (GetFileAttributesW(altPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        executePath = altPath;
                    }
                }

                if (GetFileAttributesW(executePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    return;
                }
            }

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask = SEE_MASK_FLAG_NO_UI;
            sei.lpFile = executePath.c_str();
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteExW(&sei);
        } else {
            std::wstring shellPath = L"shell:AppsFolder\\" + executePath;
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask = SEE_MASK_FLAG_NO_UI;
            sei.lpFile = shellPath.c_str();
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteExW(&sei);
        }
        return;
    }

    ShellExecuteW(nullptr, L"open", L"ms-settings:", nullptr, nullptr, SW_SHOWNORMAL);
}

// ----------------------------------------------------------------------------
// WASAPI Loopback Capture & Waveform Analyzer
// ----------------------------------------------------------------------------

inline float SampleAudioAmplitude(BYTE* data, UINT32 frames, WAVEFORMATEX* format) {
    if (!data || !frames || !format || !format->nChannels) {
        return 0.0f;
    }

    double sum = 0.0;
    size_t samples = static_cast<size_t>(frames) * format->nChannels;

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) &&
         IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,
                     kSubTypeIeeeFloat))) {
        auto* f = reinterpret_cast<float*>(data);
        for (size_t i = 0; i < samples; ++i) {
            sum += f[i] * f[i];
        }
    } else if (format->wBitsPerSample == 16) {
        auto* s = reinterpret_cast<int16_t*>(data);
        for (size_t i = 0; i < samples; ++i) {
            const double v = s[i] / 32768.0;
            sum += v * v;
        }
    }

    const double rms = samples ? std::sqrt(sum / samples) : 0.0;
    return Clamp(static_cast<float>(rms * 4.0), 0.0f, 1.0f);
}

inline void PushWaveformSample(float amplitude) {
    std::lock_guard lock(g_stateMutex);

    float lastVal = 0.0f;
    if (g_state.waveformWrite > 0) {
        lastVal = g_state.waveform[(g_state.waveformWrite - 1) % g_state.waveform.size()];
    }

    // Exponential Moving Average: Snappy attack (0.7) for beats, buttery smooth release (0.15)
    float smoothed;
    if (amplitude > lastVal) {
        smoothed = lastVal * 0.3f + amplitude * 0.7f;
    } else {
        smoothed = lastVal * 0.85f + amplitude * 0.15f;
    }

    g_state.waveform[g_state.waveformWrite % g_state.waveform.size()] = smoothed;
    ++g_state.waveformWrite;
}

inline void PushAudioChunks(BYTE* data, UINT32 frames, WAVEFORMATEX* format) {
    if (!data || !frames || !format || !format->nChannels) {
        PushWaveformSample(0.0f);
        return;
    }

    constexpr UINT32 chunkFrames = 64;
    const UINT32 channels = format->nChannels;
    const bool isFloat =
        format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) &&
         IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,
                     kSubTypeIeeeFloat));
    const bool is16 = format->wBitsPerSample == 16;

    if (!isFloat && !is16) {
        PushWaveformSample(SampleAudioAmplitude(data, frames, format));
        return;
    }

    for (UINT32 frame = 0; frame < frames; frame += chunkFrames) {
        const UINT32 chunk = std::min(chunkFrames, frames - frame);
        double sum = 0.0;
        const size_t samples = static_cast<size_t>(chunk) * channels;
        const size_t start = static_cast<size_t>(frame) * channels;

        if (isFloat) {
            auto* f = reinterpret_cast<float*>(data);
            for (size_t i = 0; i < samples; ++i) {
                const double v = f[start + i];
                sum += v * v;
            }
        } else {
            auto* s = reinterpret_cast<int16_t*>(data);
            for (size_t i = 0; i < samples; ++i) {
                const double v = s[start + i] / 32768.0;
                sum += v * v;
            }
        }

        PushWaveformSample(Clamp(static_cast<float>(std::sqrt(sum / samples) * 4.0), 0.0f, 1.0f));
    }
}

// ----------------------------------------------------------------------------
// Worker Thread Procedures
// ----------------------------------------------------------------------------

inline DWORD WINAPI MediaThreadProc(void*) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    using Manager = winrt_media::GlobalSystemMediaTransportControlsSessionManager;
    Manager manager{nullptr};
    bool loggedUnavailable = false;

    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        MediaSnapshot next;

        try {
            if (!manager) {
                manager = Manager::RequestAsync().get();
            }

            if (manager) {
                auto session = SelectActiveMediaSession(manager);
                if (session) {
                    UpdateMediaSnapshot(next, session);
                }
            }
        } catch (...) {
            if (!loggedUnavailable) {
                Wh_Log(L"WinRT media session unavailable; media module will fall back to idle.");
                loggedUnavailable = true;
            }
        }

        CommitMediaSnapshot(std::move(next));

        WaitForSingleObject(g_stopEvent, 1500);
    }

    winrt::uninit_apartment();
    return 0;
}

inline DWORD WINAPI AudioThreadProc(void*) {
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        if (!g_audioCaptureNeeded.load(std::memory_order_relaxed)) {
            WaitForSingleObject(g_stopEvent, 250);
            continue;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX* mixFormat = nullptr;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                      CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (SUCCEEDED(hr)) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }
        if (SUCCEEDED(hr)) {
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(client.GetAddressOf()));
        }
        if (SUCCEEDED(hr)) {
            hr = client->GetMixFormat(&mixFormat);
        }
        if (SUCCEEDED(hr)) {
            REFERENCE_TIME bufferDuration = 10000000 / 5;
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    bufferDuration, 0, mixFormat, nullptr);
        }
        if (SUCCEEDED(hr)) {
            hr = client->GetService(IID_PPV_ARGS(&capture));
        }
        if (SUCCEEDED(hr)) {
            hr = client->Start();
        }

        if (FAILED(hr)) {
            if (mixFormat) {
                CoTaskMemFree(mixFormat);
            }
            WaitForSingleObject(g_stopEvent, 2500);
            continue;
        }

        double lastPacketTime = NowSeconds();
        double nextWatchdogCheck = lastPacketTime + 1.0;

        while (WaitForSingleObject(g_stopEvent, 16) == WAIT_TIMEOUT) {
            if (!g_audioCaptureNeeded.load(std::memory_order_relaxed)) {
                break;
            }
            UINT32 packetFrames = 0;
            if (FAILED(capture->GetNextPacketSize(&packetFrames))) {
                break;
            }

            float amplitude = 0.0f;
            int packets = 0;
            while (packetFrames > 0) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
                    break;
                }

                lastPacketTime = NowSeconds();

                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    amplitude = std::max(amplitude, SampleAudioAmplitude(data, frames, mixFormat));
                    PushAudioChunks(data, frames, mixFormat);
                } else {
                    PushWaveformSample(0.0f);
                }
                capture->ReleaseBuffer(frames);
                ++packets;

                if (FAILED(capture->GetNextPacketSize(&packetFrames))) {
                    packetFrames = 0;
                }
            }

            if (packets > 0) {
                if (amplitude <= 0.001f) {
                    PushWaveformSample(0.0f);
                }
            } else {
                PushWaveformSample(0.0f);
            }

            const double now = NowSeconds();
            if (now >= nextWatchdogCheck) {
                nextWatchdogCheck = now + 1.0;
                bool mediaPlayingNow = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    mediaPlayingNow = g_state.media.playing;
                }
                if (mediaPlayingNow && (now - lastPacketTime) > 5.0) {
                    Wh_Log(L"Audio loopback capture appears stalled while media is playing; reconnecting...");
                    break;
                }
            }
        }

        client->Stop();
        if (mixFormat) {
            CoTaskMemFree(mixFormat);
        }
    }

    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }

    return 0;
}

} // namespace MediaEngine

#endif // MEDIA_SMTC_ENGINE_HPP
