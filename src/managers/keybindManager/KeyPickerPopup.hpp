#pragma once

#include <Geode/ui/Popup.hpp>
using namespace geode::prelude;

struct RebindWindow : SimpleEvent<RebindWindow, bool> {
    using SimpleEvent::SimpleEvent;
};
struct KeybindSender : SimpleEvent<KeybindSender, enumKeyCodes> {
    using SimpleEvent::SimpleEvent;
};

// I stole this from custom keybinds lol
// https://github.com/geode-sdk/CustomKeybinds/blob/f70ce1acaaf14d4be7ec2a42d9384d00dda928ac/src/Keybinds.cpp#L30
inline std::string keyToString(enumKeyCodes key) {
    switch (key) {
        case KEY_None:      return "(none)";
        case KEY_C:         return "C";     // rob HOW
        case KEY_Multiply:  return "Multiply";
        case KEY_Divide:    return "Divide";
        case KEY_OEMPlus:   return "Plus";
        case KEY_OEMMinus:  return "Minus";
        default: {
            auto s = CCKeyboardDispatcher::get()->keyToString(key);
            return (s != nullptr) ? s : fmt::format("(unknown: {})", int(key));
        }
    }
}

class KeyPickerPopup : public Popup {
protected:
    ListenerHandle m_keyListener;
    bool init(enumKeyCodes current, std::function<void(enumKeyCodes)> onConfirm) {
        if (!Popup::init(220.0f, 130.0f))
            return false;
        key = current;
        RebindWindow().send(true);
        this->setTitle("Press a key!", "bigFont.fnt");

        keyLabel = CCLabelBMFont::create("Press Any Key", "goldFont.fnt");
        keyLabel->setID("selected-key");

        m_mainLayer->addChildAtPosition(keyLabel, Anchor::Center, {0, 8});

        m_keyListener = KeybindSender().listen([this, onConfirm](enumKeyCodes pressedKey) {
            if (pressedKey == enumKeyCodes::KEY_Escape) pressedKey = enumKeyCodes::KEY_None;
            int k = int(pressedKey);
            if (k < 0) return;
            key = pressedKey;
            onConfirm(pressedKey);
            RebindWindow().send(false);
            m_keyListener.destroy();
            if (k != 0) this->onClose(nullptr);
        });

        //auto confirmKey = ButtonSprite::create("Confirm", "goldFont.fnt", "GJ_button_01.png");
        //confirmKey->setScale(0.8f);

        //auto confirmBtn = CCMenuItemExt::createSpriteExtra(confirmKey, [this, onConfirm] (CCObject* sender) { onConfirm(getKey()); this->onClose(nullptr); });
        //m_buttonMenu->addChildAtPosition(confirmBtn, Anchor::Bottom, {0, 26});
        
        return true;
    };
    enumKeyCodes key;
    CCLabelBMFont* keyLabel;
public:
    static KeyPickerPopup* create(enumKeyCodes current, std::function<void(enumKeyCodes)> onConfirm) {
        auto ret = new KeyPickerPopup();
        if (ret->init(current, onConfirm)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
    enumKeyCodes getKey() { return key; };
};