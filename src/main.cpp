#include <Geode/Geode.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

#define WINRT_CPPWINRT

using namespace geode::prelude;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

bool togglePlayback = false;
bool listeningViaWidget = false;
int statesChanged = 0;

bool isMediaPlaying() {
  try {
    auto mediaManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = mediaManager.GetCurrentSession();

    if (currentSession) {
      auto playbackInfo = currentSession.GetPlaybackInfo();
      auto playbackStatus = playbackInfo.PlaybackStatus();

      log::debug("SMTC playback status: {}", static_cast < int > (playbackStatus));

      if (playbackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
        return true;
      }
    }
  } catch (const hresult_error & e) {
    log::debug("Failed to get media session or playback info: {}", to_string(e.message().c_str()));
  }

  return false;
}

void pauseMediaPlayback() {
  try {
    auto mediaManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = mediaManager.GetCurrentSession();

    if (currentSession) {
      auto result = currentSession.TryPauseAsync().get();
      if (result) {
        log::debug("Media playback paused.");
      } else {
        log::debug("Failed to pause media playback.");
      }
    }
  } catch (const hresult_error & e) {
    log::debug("Failed to get media session or pause playback: {}", to_string(e.message().c_str()));
  }
}

void resumeMediaPlayback() {
  try {
    auto mediaManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = mediaManager.GetCurrentSession();
    if (currentSession) {
      auto result = currentSession.TryPlayAsync().get();
      if (result) {
        log::debug("Media playback resumed.");
      } else {
        log::debug("Failed to resume media playback.");
      }
    }
  } catch (const hresult_error & e) {
    log::debug("Failed to get media session or resume playback: {}", to_string(e.message().c_str()));
  }
}
class $modify(LevelEditorLayer) {
  void onPlaytest() {
    LevelEditorLayer::onPlaytest();
    if (Mod::get() -> getSettingValue < bool > ("toggleWhenPlaytesting") == false) {
      return;
    }
    if (isMediaPlaying() == true) {
      pauseMediaPlayback();
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
  }
  void onResumePlaytest() {
    LevelEditorLayer::onResumePlaytest();
    if (Mod::get() -> getSettingValue < bool > ("toggleWhenPlaytesting") == false) {
      return;
    }
    if (isMediaPlaying() == true) {
      pauseMediaPlayback();
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
  }
  void onStopPlaytest() {
    LevelEditorLayer::onStopPlaytest();
    if (Mod::get() -> getSettingValue < bool > ("toggleWhenPlaytesting") == false) {
      return;
    }
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
  }
  bool init(GJGameLevel * p0, bool p1) {
    if (!LevelEditorLayer::init(p0, p1)) return false;
    if (isMediaPlaying() == true) {
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
    if (Mod::get() -> getSettingValue < bool > ("resumeAtEditorEnter") == false) {
      return true;
    }
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
    return true;
  }
};
class $modify(PlayLayer) {
  bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
    if (Mod::get() -> getSettingValue < bool > ("muteAtStartLevel") == false) {
      return true;
    }
    if (isMediaPlaying() == true) {
      pauseMediaPlayback();
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
    return true;
  }
  void onQuit() {
    PlayLayer::onQuit();
    if (Mod::get() -> getSettingValue < bool > ("resumeAtEndLevel") == false) {
      return;
    }
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
  }
};

class $modify(CustomSongWidget) {
  void onPlayback(cocos2d::CCObject * sender) {
    CustomSongWidget::onPlayback(sender);
    if (Mod::get() -> getSettingValue < bool > ("muteAtWidgetPlay") == false) {
      return;
    }
    if (isMediaPlaying() == true) {
      togglePlayback = true;
      pauseMediaPlayback();
    } else {
      togglePlayback = false;
    }
  }
};