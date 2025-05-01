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
    GlobalSystemMediaTransportControlsSessionManager m_mediaManager = nullptr;
public:
    bool isWine();
    void getMediaManager();
    void removeMediaManager();
    void resumePlayback();
    void pausePlayback();
    bool isPlaybackActive();
    bool m_wine = false;

    bool m_immune = false;
    bool m_active = false;

    static PlaybackManager& get() {
        static PlaybackManager instance;
        return instance; 
    }
};