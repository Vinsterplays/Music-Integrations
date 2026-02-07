#include <Geode/geode.hpp>

using namespace geode::prelude;

class linkSpotifyPopup : public Popup {
public:
    static linkSpotifyPopup* create(int value) {
        auto popup = new linkSpotifyPopup;
        if (popup->init(value)) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

protected:
    bool init(int value) {
        if (!Popup::init(320.f, 160.f))
            return false;

        auto node = CCLabelBMFont::create("Please link your Spotify account\nto use Music Integrations", "bigFont.fnt");
        node->setScale(0.7f);
        m_mainLayer->addChildAtPosition(node, Anchor::Center);

        return true;
    }
};
