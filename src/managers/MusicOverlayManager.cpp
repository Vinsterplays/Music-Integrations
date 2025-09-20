#include "Geode/loader/Event.hpp"
#include "Geode/loader/Log.hpp"
#include "PlaybackManager.hpp"
#include "AdvancedLabelManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/ui/GeodeUI.hpp>

class MusicControlOverlay : public CCLayer {
protected:
	CCScale9Sprite* m_bg;
	Label* m_musicTitle;
    Label* m_musicArtist;
    CCLabelBMFont* m_unavailableLabel;
    CCLabelBMFont* m_autoLabel;
    CCMenu* m_menu;
    //CCMenuItemSpriteExtra* m_settingsBtn;
    CCMenuItemSpriteExtra* m_rewindBtn;
    CCMenuItemSpriteExtra* m_playbackBtn;
    CCMenuItemSpriteExtra* m_skipBtn;
    CCMenuItemToggler* m_autoBtn;
    PlaybackManager pbm = PlaybackManager::get();
    using SongUpdateListener = geode::EventListener<geode::DispatchFilter<std::string>>;
    SongUpdateListener* m_titleListener = nullptr;
    SongUpdateListener* m_artistListener = nullptr;
    using PlaybackUpdateListener = geode::EventListener<geode::DispatchFilter<bool>>;
    PlaybackUpdateListener* m_playbackListener = nullptr;
    


	bool init() override {
		if (!CCLayer::init())
			return false;
		
        this->setTouchEnabled(true);
        auto touchDispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
        touchDispatcher->addTargetedDelegate(this, -999, true);
		this->setAnchorPoint({ 1, 0 });
		this->setContentSize({ 400, 125 });
		this->setScale(.5f);
        this->setZOrder(130);

		m_bg = CCScale9Sprite::create(
			"square02b_001.png", { 0.0f, 0.0f, 80.0f, 80.0f }
		);
		m_bg->setContentSize(this->getContentSize());
		m_bg->setColor({ 0, 0, 0 });
		m_bg->setOpacity(205);
		this->addChildAtPosition(m_bg, Anchor::Center);

        if(pbm.m_mediaManager) {
            m_musicTitle = Label::create("No Song", "font_default.fnt"_spr);
            m_musicTitle->addAllFonts();
            m_musicTitle->limitLabelWidth(250.f, 1.5, 0.1f);
            this->addChildAtPosition(m_musicTitle, Anchor::Top, ccp(0, -20));

            m_musicArtist = Label::create("No Artist", "font_default.fnt"_spr);
            m_musicArtist->addAllFonts();
            m_musicArtist->setColor({253, 205, 52});
            m_musicArtist->limitLabelWidth(250.f, 1.2, 0.1f);
            this->addChildAtPosition(m_musicArtist, Anchor::Top, ccp(0, -50));

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
            this->addChildAtPosition(m_menu, Anchor::Bottom, ccp(0, this->getContentHeight()/4));
            this->addChildAtPosition(m_autoLabel, Anchor::Bottom, ccp(140, this->getContentHeight()/4));
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
        if (show) {
            int highest = CCScene::get()->getHighestChildZ();
            this->setZOrder(highest + 1);
             auto pos = AnchorLayout::getAnchoredPosition(this->getParent(), Anchor::BottomRight, ccp(5, 10));
		    this->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.75f, pos)));
        } else {
            auto pos = AnchorLayout::getAnchoredPosition(this->getParent(), Anchor::BottomRight, ccp(205, 10));
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
        m_titleListener = new EventListener([this](std::string title) {
            if (!title.empty()) this->updateTitle(title);
            return ListenerResult::Propagate;
        }, geode::DispatchFilter<std::string>("title-update"_spr));
        m_artistListener = new EventListener([this](std::string artist) {
            if (!artist.empty()) this->updateArtist(artist);
            return ListenerResult::Propagate;
        }, geode::DispatchFilter<std::string>("artist-update"_spr));
        m_playbackListener = new EventListener([this](bool status) {
            togglePlaybackBtn(status);
            return ListenerResult::Propagate;
        }, geode::DispatchFilter<bool>("playback-update"_spr));
    }

    void onExit() override {
        this->show(false);
        if (m_titleListener) {
            delete m_titleListener;
            m_titleListener = nullptr;
        }
        if (m_artistListener) {
            delete m_artistListener;
            m_artistListener = nullptr;
        }
        if (m_playbackListener) {
            delete m_playbackListener;
            m_playbackListener = nullptr;
        }
        CCLayer::onExit();
    }

public:
	static MusicControlOverlay* get() {
		if (auto existing = CCScene::get()->getChildByType<MusicControlOverlay>(0)) {
			return existing;
		}
		auto create = MusicControlOverlay::create();
		create->setID("overlay"_spr);
		CCScene::get()->addChildAtPosition(create, Anchor::BottomRight, ccp(205, 10), false);
		return create;
	}

	void updateValues(bool show) {
        if (pbm.m_mediaManager) {
            auto title = pbm.getCurrentSongTitle();
            auto artist = pbm.getCurrentSongArtist();

            m_musicTitle->setString(title.has_value() ? title->c_str() : "No Song");
            m_musicTitle->limitLabelWidth(250.f, 1.5, 0.1f);

            m_musicArtist->setString(artist.has_value() ? artist->c_str() : "No Artist");
            m_musicArtist->limitLabelWidth(250.f, 1.2, 0.1f);

            auto status = pbm.isPlaybackActive();

            m_playbackBtn->setNormalImage(CCSprite::createWithSpriteFrameName(
            status ? 
                "GJ_pauseBtn_001.png" :
                "GJ_playMusicBtn_001.png"
            ));
            m_playbackBtn->updateSprite();

            if (Mod::get()->getSavedValue<bool>("autoEnabled")) {
                m_autoBtn->toggle(true);
            }
        }
        
		this->show(show);
	}

    void updateTitle(std::string title) {
        if (!m_musicTitle || !pbm.m_mediaManager) return;

        m_musicTitle->setString(title.length() != 0 ? title.c_str() : "No Song");
        m_musicTitle->limitLabelWidth(250.f, 1.5, 0.1f);
    }

    void updateArtist(std::string artist) {
        if (!m_musicArtist || !pbm.m_mediaManager) return;

        m_musicArtist->setString(artist.length() != 0 ? artist.c_str() : "No Artist");
        m_musicArtist->limitLabelWidth(250.f, 1.2, 0.1f);
    }

    void togglePlaybackBtn(bool status) {
        if (!m_playbackBtn || !pbm.m_mediaManager) return;

        m_playbackBtn->setNormalImage(CCSprite::createWithSpriteFrameName(
            status ? 
                "GJ_pauseBtn_001.png" :
                "GJ_playMusicBtn_001.png"
        ));
        m_playbackBtn->updateSprite();
    }
};