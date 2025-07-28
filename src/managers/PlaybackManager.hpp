#pragma once

#include <Geode/Geode.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#define WINRT_CPPWINRT

using namespace geode::prelude;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

class PlaybackManager {
protected:
    PlaybackManager() {}
public:
    GlobalSystemMediaTransportControlsSessionManager m_mediaManager = nullptr;
    bool isWine();
    bool getMediaManager();
    void removeMediaManager();
    bool control(bool play);
    bool skip(bool direction);
    bool toggleControl();
    bool isPlaybackActive();
    std::optional<std::string> getCurrentSongTitle();
    std::optional<std::string> getCurrentSongArtist();
    
    bool m_wine = false;

    bool m_immune = false;
    bool m_active = false;

    static PlaybackManager& get() {
        static PlaybackManager instance;
        return instance; 
    }
};