#include "Geode/loader/Event.hpp"
#include "Geode/loader/Log.hpp"
#include "PlaybackManager.hpp"
#include "AdvancedLabelManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/Utils.hpp>
#include <arc/time/Sleep.hpp>

using namespace geode::prelude;

class MusicControlOverlay : public CCLayer {
protected:
	CCScale9Sprite* m_bg;
	Label* m_musicTitle;
    Label* m_musicArtist;
    LazySprite* m_musicImage;
    CCLabelBMFont* m_unavailableLabel;
    CCLabelBMFont* m_autoLabel;
    CCMenu* m_menu;
    //CCMenuItemSpriteExtra* m_settingsBtn;
    CCMenuItemSpriteExtra* m_rewindBtn;
    CCMenuItemSpriteExtra* m_playbackBtn;
    CCMenuItemSpriteExtra* m_skipBtn;
    CCMenuItemToggler* m_autoBtn;
    PlaybackManager& pbm = PlaybackManager::get();
    ListenerHandle m_titleListener;
    ListenerHandle m_artistListener;
    ListenerHandle m_imageListener;
    ListenerHandle m_playbackListener;

    arc::TaskHandle<void> m_pollingTask;
    int m_pollingCooldown = 0;
    bool m_pollingToggle = false;


	bool init() override {
		if (!CCLayer::init())
			return false;
		
        this->setTouchEnabled(true);
        auto touchDispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
        touchDispatcher->addTargetedDelegate(this, -999, true);
		this->setAnchorPoint({ 1, 0 });
		this->setContentSize({ 450, 125 });
		this->setScale(.5f);
        this->setZOrder(130);

		m_bg = CCScale9Sprite::create(
			"square02b_001.png", { 0.0f, 0.0f, 80.0f, 80.0f }
		);
		m_bg->setContentSize(this->getContentSize());
		m_bg->setColor({ 0, 0, 0 });
		m_bg->setOpacity(205);
		this->addChildAtPosition(m_bg, Anchor::Center);

        #ifdef GEODE_IS_WINDOWS
        if(pbm.m_mediaManager || Mod::get()->getSavedValue<bool>("hasAuthorized")) {
        #else
        if(Mod::get()->getSavedValue<bool>("hasAuthorized")) {
        #endif
            m_musicTitle = Label::create("No Song", "font_default.fnt"_spr);
            m_musicTitle->addAllFonts();
            m_musicTitle->limitLabelWidth(200.f, 1.5, 0.1f);
            m_musicTitle->setAnchorPoint({0.f, 0.5f});
            this->addChildAtPosition(m_musicTitle, Anchor::Top, ccp(-100, -25));

            m_musicArtist = Label::create("No Artist", "font_default.fnt"_spr);
            m_musicArtist->addAllFonts();
            m_musicArtist->setColor({253, 205, 52});
            m_musicArtist->limitLabelWidth(200.f, 1.2, 0.1f); 
            m_musicArtist->setAnchorPoint({0.f, 0.5f});
            this->addChildAtPosition(m_musicArtist, Anchor::Top, ccp(-100, -55));

            m_musicImage = LazySprite::create({this->getContentSize().height*0.85f, this->getContentSize().height*0.85f});
            m_musicImage->setAutoResize(true);
            this->addChildAtPosition(m_musicImage, Anchor::Left, ccp(10 + m_musicImage->getContentSize().width/2, 0));
            m_musicImage->initWithFile(fmt::format("{}/default.png", Mod::get()->getID()).c_str());

            m_menu = CCMenu::create();
            m_menu->setContentSize({this->getContentSize().width*0.75f, this->getContentSize().height/2});
            m_menu->setID("button-menu"_spr);
            m_menu->setLayout(RowLayout::create()
                ->setGap(20.f)
                ->setAxisAlignment(AxisAlignment::Center)
            );

            /*m_settingsBtn = CCMenuItemSpriteExtra::create(
                CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png"),
                this,
                menu_selector(MusicControlOverlay::onSettings)
            );*/

            m_rewindBtn = CCMenuItemSpriteExtra::create(
                CCSprite::create("previous.png"_spr),
                this,
                menu_selector(MusicControlOverlay::onPrevious)
            );

            m_playbackBtn = CCMenuItemSpriteExtra::create(
                CCSprite::createWithSpriteFrameName("GJ_playMusicBtn_001.png"),
                this,
                menu_selector(MusicControlOverlay::onPlayback)
            );

            m_skipBtn = CCMenuItemSpriteExtra::create(
                CCSprite::create("skip.png"_spr),
                this,
                menu_selector(MusicControlOverlay::onSkip)
            );

            m_autoBtn = CCMenuItemToggler::create(
                CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
                CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
                this,
                menu_selector(MusicControlOverlay::onToggleAuto)
            );

            m_autoLabel = CCLabelBMFont::create("Auto", "bigFont.fnt");
            m_autoLabel->setScale(0.7f);

            //m_menu->addChild(m_settingsBtn);
            m_menu->addChild(m_rewindBtn);
            m_menu->addChild(m_playbackBtn);
            m_menu->addChild(m_skipBtn);
            m_menu->addChild(m_autoBtn);
            m_menu->setTouchPriority(-999);
            m_menu->updateLayout();
            this->addChildAtPosition(m_menu, Anchor::Bottom, ccp(10, this->getContentHeight()/4));
            this->addChildAtPosition(m_autoLabel, Anchor::Bottom, ccp(150, this->getContentHeight()/4));
        } else {
            m_unavailableLabel = CCLabelBMFont::create("Media Controls Unavailable", "goldFont.fnt");
            this->addChildAtPosition(m_unavailableLabel, Anchor::Center);
        }

		

		return true;
	}

    /*void onSettings(CCObject*) {
        openSettingsPopup(Mod::get(), false);
    }*/

    void onPlayback(CCObject*) {
        pbm.toggleControl();
    }

    void onSkip(CCObject*) {
        pbm.skip(true);
    }

    void onPrevious(CCObject*) {
        pbm.skip(false);
    }

    void onToggleAuto(CCObject* btn) {
        auto enabled = Mod::get()->getSavedValue<bool>("autoEnabled");
        Mod::get()->setSavedValue<bool>("autoEnabled", !enabled);
    }

	static MusicControlOverlay* create() {
		auto ret = new MusicControlOverlay();
		if (ret && ret->init()) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	void show(bool show) {
		this->stopAllActions();
        auto anchorStr = Mod::get()->getSettingValue<std::string>("overlay_anchor");

        Anchor anchor = Anchor::BottomRight;
        int offsetX = 5;
        int offsetY = 10;
        int slideLenth = 235;

        if (anchorStr == "Top Left") {
            anchor = Anchor::TopLeft;
            offsetX = 5;
            offsetY = -10;
            this->setAnchorPoint({ 0, 1 });
            slideLenth = -235;
        }
        else if (anchorStr == "Top Right") {
            anchor = Anchor::TopRight;
            offsetX = -5;
            offsetY = -10;
            this->setAnchorPoint({ 1, 1 });
            slideLenth = 235;
        }
        else if (anchorStr == "Bottom Left") {
            anchor = Anchor::BottomLeft;
            offsetX = 5;
            offsetY = 10;
            this->setAnchorPoint({ 0, 0 });
            slideLenth = -235;
        }
        else if (anchorStr == "Bottom Right") {
            anchor = Anchor::BottomRight;
            offsetX = -5;
            offsetY = 10;
            this->setAnchorPoint({ 1, 0 });
            slideLenth = 235;
        }
        else if (anchorStr == "Middle Left") {
            anchor = Anchor::Left;
            offsetX = 5;
            offsetY = 0;
            this->setAnchorPoint({ 0, 0.5 });
            slideLenth = -235;
        }
        else if (anchorStr == "Middle Right") {
            anchor = Anchor::Right;
            offsetX = -5;
            offsetY = 0;
            this->setAnchorPoint({ 1, 0.5 });
            slideLenth = 235;
        }

        if (show) {
            int highest = CCScene::get()->getHighestChildZ();
            this->setZOrder(highest + 1);
            
            this->setPosition(AnchorLayout::getAnchoredPosition(CCScene::get(), anchor, ccp(offsetX + slideLenth, offsetY)));
             auto pos = AnchorLayout::getAnchoredPosition(CCScene::get(), anchor, ccp(offsetX, offsetY));
		    this->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.75f, pos)));
        } else {
            auto pos = AnchorLayout::getAnchoredPosition(CCScene::get(), anchor, ccp(offsetX + slideLenth, offsetY));
		    this->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.75f, pos)));
        }
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        auto touchPosWorld = touch->getLocation();
        auto touchPos = this->convertToNodeSpace(touchPosWorld);

        CCRect bounds = { 0.f, 0.f, this->getContentSize().width, this->getContentSize().height };
        if (!bounds.containsPoint(touchPos))
            return false;

        if (auto menu = this->getChildByType<CCMenu>(0)){
            for (auto item : CCArrayExt<CCMenuItem*>(menu->getChildren())) {
                if (!item->isVisible() || !item->isEnabled()) continue;

                auto local = item->convertToNodeSpace(touchPosWorld);
                CCRect itemBounds = { 0.f, 0.f, item->getContentSize().width, item->getContentSize().height };

                if (itemBounds.containsPoint(local)) {
                    return false;
                }
            }
        }

        return true;
    }

    void onEnter() override {
        CCLayer::onEnter();
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -999, true);
        m_titleListener = PlaybackManager::SongUpdateEvent("title-update"_spr).listen([this](auto title) {
            if (!title.empty()) this->updateTitle(title);
            return ListenerResult::Propagate;
        });
        m_artistListener = PlaybackManager::SongUpdateEvent("artist-update"_spr).listen([this](auto artist) {
            if (!artist.empty()) this->updateArtist(artist);
            return ListenerResult::Propagate;
        });

        if (!pbm.isWindows()) {
            m_imageListener = PlaybackManager::SongUpdateEvent("image-update"_spr).listen([this](auto image) {
                if (!image.empty()) this->updateImageFromUrl(image);
                return ListenerResult::Propagate;
            });
        }
        m_playbackListener = PlaybackManager::PlaybackUpdateEvent("playback-update"_spr).listen([this](bool status) {
            togglePlaybackBtn(status);
        });
        if(Mod::get()->getSavedValue<bool>("hasAuthorized") && !pbm.isWindows()) {
            std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
            if (token.empty()) {
                log::error("No Spotify token available");
                return;
            }

            PlaybackManager::get().spotifyGetPlaybackInfo(token, 0);

            arc::Notify notify;

            async::spawn([notify] -> arc::Future<> {
                while (true) {
                    notify.notifyOne();
                    co_await arc::sleep(asp::Duration::fromSecs(1));
                }
            });

            m_pollingTask = async::spawn([notify, token, this] -> arc::Future<> {
                while (true) {
                    co_await notify.notified();
                    if (m_pollingCooldown > 0) m_pollingCooldown -= 1;
                    
                    if (m_pollingCooldown <= 0 && m_pollingToggle &&Mod::get()->getSavedValue<bool>("hasAuthorized")) {
                        PlaybackManager::get().spotifyGetPlaybackInfo(token, 0);
                        m_pollingCooldown = 4;
                    }
                }
            });
        }
    }

    void onExit() override {
        this->show(false);
        m_titleListener.destroy();
        m_artistListener.destroy();
        m_playbackListener.destroy();
        if (m_pollingTask.isValid()) m_pollingTask.abort();
        CCLayer::onExit();
    }

public:
	static MusicControlOverlay* get() {
		if (auto existing = OverlayManager::get()->getChildByType<MusicControlOverlay>(0)) {
			return existing;
		}
		auto create = MusicControlOverlay::create();
		create->setID("overlay"_spr);
        create->setPosition(AnchorLayout::getAnchoredPosition(CCScene::get(), Anchor::BottomRight, ccp(230, 10)));
		//CCScene::get()->addChildAtPosition(create, Anchor::BottomRight, ccp(205, 10), false);
        OverlayManager::get()->addChild(create);
		return create;
	}

	void updateValues(bool show) {
        #ifdef GEODE_IS_WINDOWS
        if (pbm.m_mediaManager) {
            auto title = pbm.getCurrentSongTitle();
            auto artist = pbm.getCurrentSongArtist();

            m_musicTitle->setString(title.has_value() ? title->c_str() : "No Song");
            m_musicTitle->limitLabelWidth(200.f, 1.5, 0.1f);

            m_musicArtist->setString(artist.has_value() ? artist->c_str() : "No Artist");
            m_musicArtist->limitLabelWidth(200.f, 1.2, 0.1f);
            
            pbm.isPlaybackActive([this](bool isPlaying) {
                auto status = isPlaying;
                m_playbackBtn->setNormalImage(CCSprite::createWithSpriteFrameName(
                status ? 
                    "GJ_pauseBtn_001.png" :
                    "GJ_playMusicBtn_001.png"
                ));
                m_playbackBtn->updateSprite();
            });


        }
        #endif
        if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !pbm.isWindows()) {
            m_pollingToggle = show;
            if (m_pollingCooldown == 0) {
                std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
                if (token.empty()) {
                    log::error("No Spotify token available");
                    return;
                }

                PlaybackManager::get().spotifyGetPlaybackInfo(token, 0);
                m_pollingCooldown = 4;
            }
        }
        if (Mod::get()->getSavedValue<bool>("autoEnabled")) {
            m_autoBtn->toggle(true);
        }
		this->show(show);
	}

    void updateTitle(std::string title) {
        if (!m_musicTitle) return;

        m_musicTitle->setString(title.length() != 0 ? title.c_str() : "No Song");
        m_musicTitle->limitLabelWidth(200.f, 1.5, 0.1f);
    }

    void updateArtist(std::string artist) {
        if (!m_musicArtist) return;

        m_musicArtist->setString(artist.length() != 0 ? artist.c_str() : "No Artist");
        m_musicArtist->limitLabelWidth(200.f, 1.2, 0.1f);
    }

    void updateImageFromUrl(std::string url) {
        if (!m_musicImage || url.empty()) return;

        this->removeChild(m_musicImage);
        m_musicImage = LazySprite::create({this->getContentSize().height*0.85f, this->getContentSize().height*0.85f}, true);
        m_musicImage->setZOrder(1);
        this->addChildAtPosition(m_musicImage, Anchor::Left, ccp(10 + m_musicImage->getContentSize().width/2, 0));
        m_musicImage->setAutoResize(true);
        m_musicImage->loadFromUrl(url);
    }

    void togglePlaybackBtn(bool status) {
        if (!m_playbackBtn) return;

        m_playbackBtn->setNormalImage(CCSprite::createWithSpriteFrameName(
            status ? 
                "GJ_pauseBtn_001.png" :
                "GJ_playMusicBtn_001.png"
        ));
        m_playbackBtn->updateSprite();
    }
};