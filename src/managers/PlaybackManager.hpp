#pragma once

#include <Geode/Geode.hpp>
#ifdef GEODE_IS_WINDOWS
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#endif
#include "Geode/loader/Event.hpp"
#include <chrono>

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
    async::TaskHolder<web::WebResponse> m_pollListener;
    async::TaskHolder<web::WebResponse> m_controlListener;

    int m_maxErrorLimit = 10;
    std::chrono::seconds POLL_INTERVAL = std::chrono::seconds(4);
    std::chrono::seconds DEFAULT_RETRY_AFTER = std::chrono::seconds(30);
    int m_spotifyErrorCount = 0;
    bool m_spotifyStopped = false;
    std::chrono::steady_clock::time_point m_lastPoll{};
    std::chrono::steady_clock::time_point m_retryAfter{};

    int m_controlSpamLimit = 8;
    int m_controlSpamCount = 0;
    bool m_controlSpamBlocked = false;
    std::chrono::steady_clock::time_point m_lastControlDecay{};
    bool spotifyRegisterControl();

    bool spotifyCanRequest(bool isPoll);
    void spotifyOnSuccess();
    void spotifyOnError(std::string_view reason);
    bool spotifyHandleRateLimit(web::WebResponse const& value);

    void spotifyControlRequest(std::string token, int retryCount = 0, bool play = true, bool checkState = true);
    void spotifyisPlaybackActive(std::string token, std::function<void(bool)> callback,int retryCount = 0);
    void spotifySkipRequest(std::string token, int retryCount = 0, bool direction = true);
public:
    #ifdef GEODE_IS_WINDOWS
    GlobalSystemMediaTransportControlsSessionManager m_mediaManager = nullptr;
    void removeMediaManager();
    bool getMediaManager();
    #endif
    bool isWindows();
    bool control(bool play);
    bool skip(bool direction);
    bool toggleControl();
    void isPlaybackActive(std::function<void(bool)> callback);
    std::optional<std::string> getCurrentSongTitle();
    std::optional<std::string> getCurrentSongArtist();
    void getCurrentSongThumbnail(std::function<void(std::vector<uint8_t>)> callback);
    void spotifyGetPlaybackInfo(std::string token, int retryCount = 0);
    void pollSpotify(bool force = false);
    void resumeSpotify();

    bool m_spotifyPolling = false;
    bool m_immune = false;
    bool m_active = false;

    struct SongUpdateEvent : Event<SongUpdateEvent, bool(std::string), std::string> {
        using Event::Event;
    };
    struct ThumbnailUpdateEvent : Event<ThumbnailUpdateEvent, bool(std::vector<uint8_t>), std::string> {
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