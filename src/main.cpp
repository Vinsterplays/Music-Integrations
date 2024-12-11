#include <Geode/Geode.hpp>

#include <winrt/Windows.Foundation.h>

#include <winrt/Windows.Media.Control.h>

#include <Geode/modify/LevelEditorLayer.hpp>

#include <Geode/modify/EditorPauseLayer.hpp>

#include <Geode/modify/PlayLayer.hpp>

#define WINRT_CPPWINRT

using namespace geode::prelude;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

bool togglePlayback = false;
bool m_lastMusicState = false;
bool m_isPlaytesting = false;
bool m_hitPercentage = false;

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

class $modify(PlayLayer) {
  bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
    m_hitPercentage = false;
    if (isMediaPlaying() == true) {
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
    if (Mod::get() -> getSettingValue < bool > ("muteAtStartLevel") == false) {
      return true;
    }
    if (togglePlayback == true) {
      pauseMediaPlayback();
    }
    return true;
  }
  void onQuit() {
    PlayLayer::onQuit();
    m_hitPercentage = false;
    if (Mod::get() -> getSettingValue < bool > ("resumeAtEndLevel") == false) {
      return;
    }
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
  }
  void postUpdate(float dt) {
    PlayLayer::postUpdate(dt);
    if (Mod::get() -> getSettingValue < int64_t > ("playbackPercentage") == -1) {
      return;
    }
    if (!m_hitPercentage) {
      int percent = PlayLayer::getCurrentPercentInt();
      if (percent == Mod::get() -> getSettingValue < int64_t > ("playbackPercentage")) {
        pauseMediaPlayback();
        m_hitPercentage = true;
      }
    }
  }
  void resetLevel() {
    PlayLayer::resetLevel();
    if (Mod::get() -> getSettingValue < int64_t > ("playbackPercentage") != -1) {
      if (m_hitPercentage == true) {
        m_hitPercentage = false;
        if (togglePlayback == true) {
          resumeMediaPlayback();
        }
      }
    }

  }
};

class $modify(LevelEditorLayer) {
  void updateEditor(float p0) {
    LevelEditorLayer::updateEditor(p0);
    auto * engine = FMODAudioEngine::sharedEngine();
    bool isCurrentlyPlaying = engine -> isMusicPlaying(0);
    if (m_isPlaytesting == true) {
      isCurrentlyPlaying = true;
    }
    if (isCurrentlyPlaying != m_lastMusicState) {
      if (isCurrentlyPlaying) {
        if (isMediaPlaying() == true) {
          pauseMediaPlayback();
          togglePlayback = true;
        } else {
          togglePlayback = false;
        }
      } else {
        if (togglePlayback == true) {
          resumeMediaPlayback();
        }
      }
      m_lastMusicState = isCurrentlyPlaying;
    }
  }
  void onPlaytest() {
    LevelEditorLayer::onPlaytest();
    m_isPlaytesting = true;
  }
  void onResumePlaytest() {
    LevelEditorLayer::onResumePlaytest();
    m_isPlaytesting = true;
  }
  void onStopPlaytest() {
    LevelEditorLayer::onStopPlaytest();
    m_isPlaytesting = false;
  }
  bool init(GJGameLevel * p0, bool p1) {
    if (!LevelEditorLayer::init(p0, p1)) return false;
    if (togglePlayback == true) {
      resumeMediaPlayback();
    } else {
      togglePlayback = false;
    }
    return true;
  }
};