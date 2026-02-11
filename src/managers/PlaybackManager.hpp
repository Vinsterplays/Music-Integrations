#pragma once

#include <Geode/Geode.hpp>
#ifdef GEODE_IS_WINDOWS
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#endif
#include "Geode/loader/Event.hpp"

#define WINRT_CPPWINRT

using namespace geode::prelude;
#ifdef GEODE_IS_WINDOWS
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;
#endif

class PlaybackManager {
protected:
    PlaybackManager() = default;
    async::TaskHolder<web::WebResponse> m_listener;
    void spotifyControlRequest(std::string token, int retryCount = 0, bool play = true);
    void spotifyisPlaybackActive(std::string token, std::function<void(bool)> callback,int retryCount = 0);
    void spotifySkipRequest(std::string token, int retryCount = 0, bool direction = true);
public:
    #ifdef GEODE_IS_WINDOWS
    GlobalSystemMediaTransportControlsSessionManager m_mediaManager = nullptr;
    #endif
    bool isWindows();
    bool getMediaManager();
    #ifdef GEODE_IS_WINDOWS
    void removeMediaManager();
    #endif
    bool control(bool play);
    bool skip(bool direction);
    bool toggleControl();
    void isPlaybackActive(std::function<void(bool)> callback);
    std::optional<std::string> getCurrentSongTitle();
    std::optional<std::string> getCurrentSongArtist();
    void spotifyGetPlaybackInfo(std::string token, int retryCount = 0);

    bool m_immune = false;
    bool m_active = false;

    struct SongUpdateEvent : Event<SongUpdateEvent, bool(std::string), std::string> {
        using Event::Event;
    };
    struct PlaybackUpdateEvent : ThreadSafeEvent<PlaybackUpdateEvent, bool(bool status), std::string> {
        using ThreadSafeEvent::ThreadSafeEvent;
    };
    struct RateLimitUpdate : Event<RateLimitUpdate, bool(), std::string> {
        using Event::Event;
    };

    static PlaybackManager& get() {
        static PlaybackManager instance;
        return instance;
    }
    
    PlaybackManager(const PlaybackManager&) = delete;
    PlaybackManager& operator=(const PlaybackManager&) = delete;
};