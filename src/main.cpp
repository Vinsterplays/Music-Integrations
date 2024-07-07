#include <Geode/Geode.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#define WINRT_CPPWINRT

using namespace geode::prelude;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

bool togglePlayback = false;

bool isMediaPlaying() {
  try {
    auto mediaManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = mediaManager.GetCurrentSession();

    if (currentSession) {
      auto playbackInfo = currentSession.GetPlaybackInfo();
      auto playbackStatus = playbackInfo.PlaybackStatus();

      log::debug("SMTC playback status: {}", static_cast<int>(playbackStatus));

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
    if (isMediaPlaying() == true) {
      pauseMediaPlayback();
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
  }
  void onResumePlaytest() {
    LevelEditorLayer::onResumePlaytest();
    if (isMediaPlaying() == true) {
      pauseMediaPlayback();
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
  }
  void onStopPlaytest() {
    LevelEditorLayer::onStopPlaytest();
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
  }
  bool init(GJGameLevel * p0, bool p1) {
    if (!LevelEditorLayer::init(p0, p1)) return false;
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
    if (isMediaPlaying() == true) {
      togglePlayback = true;
    } else {
      togglePlayback = false;
    }
    return true;
  }
};
class $modify(PlayLayer) {
  bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
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
    if (togglePlayback == true) {
      resumeMediaPlayback();
    }
  }
};