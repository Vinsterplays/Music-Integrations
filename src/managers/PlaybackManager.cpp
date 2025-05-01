#include "PlaybackManager.hpp"

bool PlaybackManager::isWine() {
    static bool result = false;

    HMODULE hModule = GetModuleHandleA("ntdll.dll");
    if (!hModule) {
        return false;
    }

    FARPROC func = GetProcAddress(hModule, "wine_get_version");
    result = func != nullptr;
    return result;
}

void PlaybackManager::getMediaManager() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (isWine()) {
        log::debug("Running on Wine, skipping media manager initialization.");
        m_wine = true;
        return;
    }

    auto async = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto status = async.Status();

        if (status == AsyncStatus::Completed) {
            try {
                m_mediaManager = async.GetResults();
            } catch (const winrt::hresult_error & e) {
                log::debug("Failed to get media session: {}", winrt::to_string(e.message()));
            } catch (...) {
                log::debug("Unknown exception while retrieving media session.");
            }
            break;
        }
        
        if (status == AsyncStatus::Error || status == AsyncStatus::Canceled) {
            log::debug("Failed to retrieve media manager.");
            break;
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(3)) {
            log::debug("Timed out after 3 seconds. Aborting request.");
            async.Cancel();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void PlaybackManager::removeMediaManager() {
    m_mediaManager = nullptr;
}

bool PlaybackManager::isPlaybackActive() {
    if (m_wine) return false;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();

        if (currentSession) {
            auto playbackInfo = currentSession.GetPlaybackInfo();
            auto playbackStatus = playbackInfo.PlaybackStatus();

            if (playbackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
                return true;
            }
        }
    } catch (const hresult_error & e) {
        log::debug("Failed to get media session or playback info: {}", to_string(e.message().c_str()));
    }
    return false;
}

void PlaybackManager::resumePlayback() {
    if (m_wine) return;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (currentSession && !m_immune) {
            if (m_active) {
                if (PlaybackManager::isPlaybackActive()) m_active = true; else m_active = false;
                currentSession.TryPlayAsync().Completed([](auto&& asyncOp, auto&& status) {
                });
            }
        }
    } catch (const hresult_error & e) {
        log::debug("Failed to get media session or resume playback: {}", to_string(e.message().c_str()));
    }
}

void PlaybackManager::pausePlayback() {
    if (m_wine) return;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();

        if (currentSession && !m_immune) {
            if (PlaybackManager::isPlaybackActive()) m_active = true; else m_active = false;
            if (m_active) {
                currentSession.TryPauseAsync().Completed([](auto&& asyncOp, auto&& status) {
                });
            }
        }
    } catch (const hresult_error & e) {
        log::debug("Failed to get media session or pause playback: {}", to_string(e.message().c_str()));
    }
}