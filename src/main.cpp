#include "Geode/loader/GameEvent.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "managers/MusicOverlayManager.cpp"
#include "managers/PlaybackManager.hpp"

#include <Windows.h>



#define WINRT_CPPWINRT

bool m_isLoaded = false;

$on_mod(Loaded) {
    PlaybackManager::get().getMediaManager();

    auto dummy = Label::create("", "font_default.fnt"_spr);
    dummy->addAllFonts();

    new geode::EventListener(+[](geode::GameEvent*) {
        m_isLoaded = true;
    }, geode::GameEventFilter(geode::GameEventType::Loaded));
}

$on_mod(DataSaved) {
    PlaybackManager::get().removeMediaManager();
    log::debug("Goodbye!");
}


//End goal is to stop relying on this
class $modify(FMODAudioEngine) {
    void update(float p0) {
        FMODAudioEngine::update(p0);
        if (!Mod::get()->getSavedValue<bool>("autoEnabled") || PlayLayer::get()) return;
        auto lel = LevelEditorLayer::get();
        if (lel && lel->m_playbackActive) return;
        static bool lastMusicState = false;

        bool currentMusicState = FMODAudioEngine::isMusicPlaying(0);
        if (currentMusicState != lastMusicState) {
            PlaybackManager pbm = PlaybackManager::get();
            if (currentMusicState) {
                pbm.control(false);
            } else {
                pbm.control(true);
            }
            lastMusicState = currentMusicState;
        }

    }
};

class $modify (MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        if (key == enumKeyCodes::KEY_Alt
            && down
            && !repeat
            && m_isLoaded
        ) {
            MusicControlOverlay::get()->updateValues(true);
        } else if (key == enumKeyCodes::KEY_Alt
            && !down
            && m_isLoaded
        ) {
            MusicControlOverlay::get()->updateValues(false);
        } 
        #ifdef DEBUG_BUILD
        else if (key == enumKeyCodes::KEY_E && down) {
            auto pbm = PlaybackManager::get();
            log::debug("enabled: {}", Mod::get()->getSavedValue<bool>("autoEnabled"));
            log::debug("active: {}", pbm.m_active);
            log::debug("immune: {}", pbm.m_immune);
        }
        #endif
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
    }
};

class $modify(CCScene) {
    int getHighestChildZ() {
        if(auto mco = CCScene::get()->getChildByType<MusicControlOverlay>(0)) {
            mco->setZOrder(0);
            int result = CCScene::getHighestChildZ();
            result > 130 ? mco->setZOrder(result) : mco->setZOrder(130);
            return result;
        } else {
            return CCScene::getHighestChildZ();
        }
    }
};

class $modify(LevelEditorLayer) {
    void onPlaytest() {        
        LevelEditorLayer::onPlaytest();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        auto& pbm = PlaybackManager::get();
        pbm.control(false);
        pbm.m_immune = true;
    }
    void onStopPlaytest() {
        auto playtest = GJBaseGameLayer::get()->m_playbackMode;
        LevelEditorLayer::onStopPlaytest();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled") || static_cast<int>(playtest) == 0) return;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) pbm.control(true);
    }
    bool init(GJGameLevel* p0, bool p1) {
        if (!LevelEditorLayer::init(p0, p1)) return false;
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return true;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        pbm.control(true);
        return true;
    }
};

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return true;
        auto& pbm = PlaybackManager::get();
        pbm.control(false);
        pbm.m_immune = true;
        return true;
    }
    void onQuit() {
        PlayLayer::onQuit();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) pbm.control(true);
    }
};

class $modify (EditorUI) {
    void onPlayback(CCObject* sender) {
        EditorUI::onPlayback(sender);
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        PlaybackManager pbm = PlaybackManager::get();
        if (EditorUI::m_playbackActive) {
            pbm.control(false);
        } else {
            pbm.control(true);
        }
    }
};