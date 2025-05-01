#include "managers/PlaybackManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#define WINRT_CPPWINRT

bool togglePlayback = false;
bool m_lastMusicState = false;
bool m_isPlaytesting = false;
bool m_hitPercentage = false;

$on_mod(Loaded) {
    PlaybackManager::get().getMediaManager();
}

$on_mod(DataSaved) {
    PlaybackManager::get().removeMediaManager();
    log::debug("Goodbye!");
}

class $modify(FMODAudioEngine) {
    void update(float p0) {
        static bool lastMusicState = false;

        bool currentMusicState = FMODAudioEngine::isMusicPlaying(0);
        if (currentMusicState != lastMusicState) {
            auto& pbm = PlaybackManager::get();
            if (currentMusicState) {
                pbm.pausePlayback();
            } else {
                pbm.resumePlayback();
            }
            lastMusicState = currentMusicState;
        }

        FMODAudioEngine::update(p0);
    }
};

class $modify(LevelEditorLayer) {
    void onPlaytest() {        
        LevelEditorLayer::onPlaytest();
        auto& pbm = PlaybackManager::get();
        pbm.pausePlayback();
        pbm.m_immune = true;
    }
    void onStopPlaytest() {
        LevelEditorLayer::onStopPlaytest();
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) {
            pbm.resumePlayback();
        }
    }
};

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        auto& pbm = PlaybackManager::get();
        pbm.pausePlayback();
        pbm.m_immune = true;
        return true;
    }
    void onQuit() {
        PlayLayer::onQuit();
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) {
            pbm.resumePlayback();
        }
    }
};