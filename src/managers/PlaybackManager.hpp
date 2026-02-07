#pragma once

#include <Geode/Geode.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include "Geode/loader/Event.hpp"

#define WINRT_CPPWINRT

using namespace geode::prelude;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

class PlaybackManager {
protected:
    PlaybackManager() = default;
    async::TaskHolder<web::WebResponse> m_listener;
    void spotifyControlRequest(std::string token, int retryCount = 0, bool play = true);
    void spotifyisPlaybackActive(std::string token, std::function<void(bool)> callback,int retryCount = 0);
    void spotifySkipRequest(std::string token, int retryCount = 0, bool direction = true);
public:
    GlobalSystemMediaTransportControlsSessionManager m_mediaManager = nullptr;
    bool isWindows();
    bool getMediaManager();
    void removeMediaManager();
    bool control(bool play);
    bool skip(bool direction);
    bool toggleControl();
    void isPlaybackActive(std::function<void(bool)> callback);
    std::optional<std::string> getCurrentSongTitle();
    std::optional<std::string> getCurrentSongArtist();

    bool m_immune = false;
    bool m_active = false;

    struct SongUpdateEvent : Event<SongUpdateEvent, bool(std::string), std::string> {
        using Event::Event;
    };
    struct PlaybackUpdateEvent : SimpleEvent<PlaybackUpdateEvent, bool> {
        using SimpleEvent::SimpleEvent;
    };

    static PlaybackManager& get() {
        static PlaybackManager instance;
        return instance;
    }
    
    PlaybackManager(const PlaybackManager&) = delete;
    PlaybackManager& operator=(const PlaybackManager&) = delete;
};