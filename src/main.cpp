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
#include <Geode/modify/CCDirector.hpp>

#include "managers/MusicOverlayManager.cpp"
#include "managers/PlaybackManager.hpp"
#include "managers/keybindManager/KeyPickerPopup.hpp"
#include "managers/keybindManager/keybindManager.hpp"
#include "ui/linkSpotify.hpp"
#include "managers/httpManager.hpp"

#ifdef GEODE_IS_WINDOWS

#include <Windows.h>

#endif

#define WINRT_CPPWINRT

bool m_isLoaded = false;
bool m_rebindWindowOpen = false;
int m_rateLimitCounter = 0;
async::TaskHolder<web::WebResponse> m_listener;

enumKeyCodes getOverlayKey()   { return Mod::get()->getSettingValue<Keybind>("key_overlay").getKey(); };

$on_mod(Loaded) {
    PlaybackManager::get().getMediaManager();

    auto dummy = Label::create("", "font_default.fnt"_spr);
    dummy->addAllFonts();

    if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !PlaybackManager::get().isWindows()) {
        SpotifyAuth::get()->refresh([](std::string newToken) {
            log::debug("Token refreshed on load");
        });
    }

    GameEvent(geode::GameEventType::Loaded).listen([] {
        m_isLoaded = true;
        if (Mod::get()->getSavedValue<bool>("hasAuthorized") == false && PlaybackManager::get().isWindows() == false) {
            createQuickPopup("Link Spotify", "Please link your Spotify account to use Music Integrations <cc>(You will have 2 minutes to authenticate)</c>", "Link Now", "Close",
                [](auto, bool btn2) {
                    if (btn2) return;
                    SpotifyAuth::get()->start([](std::string code) {
                        auto req = web::WebRequest();

                        m_listener.spawn(
                            req.post("https://craftify.thatgravyboat.tech/api/v1/public/auth?type=auth&code=" + code, geode::getMod()),
                            [](web::WebResponse value) {
                                auto jsonResult = value.json();
                                if (!jsonResult.isOk()) {
                                    log::error("Failed to parse JSON response");
                                    return;
                                }
                                
                                auto jsonUnwrap = jsonResult.unwrap();
                                if (!jsonUnwrap.contains("access_token")) {
                                    log::error("No access_token in response!");
                                    return;
                                }
                                
                                auto tokenResult = jsonUnwrap["access_token"].asString();
                                if (!tokenResult.isOk()) {
                                    log::error("Failed to get access_token as string");
                                    return;
                                }

                                auto refreshResult = jsonUnwrap["refresh_token"].asString();
                                if (!refreshResult.isOk()) {
                                    log::error("Failed to get refresh_token as string");
                                    return;
                                }
                                
                                std::string token = tokenResult.unwrap();
                                std::string refresh = refreshResult.unwrap();
                                Mod::get()->setSavedValue("spotify-token", token);
                                Mod::get()->setSavedValue("spotify-refresh", refresh);
                                log::info("Successfully saved Spotify token");
                                Mod::get()->setSavedValue("hasAuthorized", true);
                            }
                        );
                    });
                    web::openLinkInBrowser("https://accounts.spotify.com/authorize?client_id=7314a0ab3c734b2caa4b483032cdd91f&response_type=code&redirect_uri=http://127.0.0.1:21851/&scope=user-read-playback-state user-modify-playback-state");
                }
            );
        }

        arc::Notify notify;

        PlaybackManager::RateLimitUpdate("rate-limit-update"_spr).listen([] {
            log::debug("Rate limit update received");
            if(Mod::get()->getSavedValue<bool>("isRateLimited")) {
                log::debug("rate limited!");
                return;
            }
            m_rateLimitCounter += 1;
            if (m_rateLimitCounter >= 10) {
                log::debug("Rate limit hit! {}", m_rateLimitCounter);
                Mod::get()->setSavedValue<bool>("isRateLimited", true);
                CCSprite* warning = CCSprite::create("/foxy_1.png"_spr);
                CCSize winSize = CCDirector::get()->getWinSize();
                float scaleRatio = (winSize.height / warning->getContentSize().height);
                warning->setScaleX(scaleRatio);
                warning->setScaleY(scaleRatio);
                CCScene::get()->addChildAtPosition(warning, Anchor::Center, {0, 0}, false);
                CCArray* frames = CCArray::create();
                for (int i = 1; i <= 14; i++) {
                    auto filename = fmt::format("/foxy_{}.png"_spr, i);
                    auto texture = CCTextureCache::sharedTextureCache()->addImage(filename.c_str(), false);
                    if (texture) {
                        auto frame = CCSpriteFrame::createWithTexture(texture, CCRectMake(0, 0, texture->getContentSize().width, texture->getContentSize().height));
                        frames->addObject(frame);
                    }
                }

                CCAnimation* anim = CCAnimation::createWithSpriteFrames(frames, 0.1f);
                CCAnimate* animate = CCAnimate::create(anim);

                warning->runAction(
                    CCSequence::create(
                        CallFuncExt::create([]() {
                            FMODAudioEngine::sharedEngine()->playEffect("/fnafjumpscare.ogg"_spr);
                        }),
                        animate,
                        CallFuncExt::create([warning]() {
                            warning->removeFromParent();
                        }),
                        nullptr
                    )
                );
            }
        }).leak();

        async::spawn([notify] -> arc::Future<> {
            while (true) {
                notify.notifyOne();
                co_await arc::sleep(asp::Duration::fromMillis(1000));
            }
        });

        async::spawn([notify] -> arc::Future<> {
            while (true) {
                co_await notify.notified();
                if (m_rateLimitCounter > 0) {
                    m_rateLimitCounter -= 1;
                    log::debug("Rate limit counter: {}", m_rateLimitCounter);
                }
                if (Mod::get()->getSavedValue<bool>("isRateLimited") && m_rateLimitCounter == 0) {
                    log::debug("No longer rate limited!");
                    Mod::get()->setSavedValue("isRateLimited", false);
                }
            }
        });

    }).leak();
    RebindWindow("rebind-window"_spr).listen([](bool open) {
        m_rebindWindowOpen = open;
    }).leak();    
}

#ifdef GEODE_IS_WINDOWS
class $modify(CCDirector) {
    void end() {
        CCDirector::end();
        PlaybackManager::get().removeMediaManager();
        log::debug("Goodbye!");
    }
};
#endif


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
            PlaybackManager& pbm = PlaybackManager::get();
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
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double p3) {
        if (m_rebindWindowOpen) {
            KeybindSender("keybind-sender"_spr).send(key);
        } else if (key == getOverlayKey()
            && down
            && !repeat
            && m_isLoaded
        ) {
            MusicControlOverlay::get()->updateValues(true);
        } else if (key == getOverlayKey()
            && !down
            && m_isLoaded
        ) {
            MusicControlOverlay::get()->updateValues(false);
        } 
        #ifdef DEBUG_BUILD
        else if (key == enumKeyCodes::KEY_E && down) {
            PlaybackManager& pbm = PlaybackManager::get();
            log::debug("enabled: {}", Mod::get()->getSavedValue<bool>("autoEnabled"));
            log::debug("active: {}", pbm.m_active);
            log::debug("immune: {}", pbm.m_immune);
        }
        #endif
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, p3);
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
        PlaybackManager& pbm = PlaybackManager::get();
        if (EditorUI::m_playbackActive) {
            pbm.control(false);
        } else {
            pbm.control(true);
        }
    }
};