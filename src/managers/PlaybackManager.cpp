#include "PlaybackManager.hpp"
#include "Geode/loader/Event.hpp"
#include "MusicOverlayManager.cpp"

using SongUpdateEvent = DispatchEvent<std::string>;
using PlaybackUpdateEvent = DispatchEvent<bool>;

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

bool PlaybackManager::getMediaManager() {
    if (isWine()) {
        log::debug("Running on Wine, skipping media manager initialization.");
        m_wine = true;
        return false;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    auto async = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto status = async.Status();

        if (status == AsyncStatus::Completed) {
            try {
                m_mediaManager = async.GetResults();
                if (auto session = m_mediaManager.GetCurrentSession()) {
                    session.MediaPropertiesChanged(
                        [this](auto sess, auto) {
                            Loader::get()->queueInMainThread([this, sess] {
                                static std::string lastTitle;
                                static std::string lastArtist;
                                auto props = sess.TryGetMediaPropertiesAsync().get();
                                auto title = winrt::to_string(props.Title());
                                auto artist = winrt::to_string(props.Artist());
                                if (title != lastTitle) {
                                    lastTitle = title;
                                    SongUpdateEvent("title-update"_spr, title).post();
                                }
                                if (artist != lastArtist) {
                                    lastArtist = artist;
                                    SongUpdateEvent("artist-update"_spr, artist).post();
                                }
                            });
                        }
                    );
                    session.PlaybackInfoChanged(
                        [this](auto sess, auto) {
                            Loader::get()->queueInMainThread([this, sess] {
                                static GlobalSystemMediaTransportControlsSessionPlaybackStatus lastStatus;
                                auto info = sess.GetPlaybackInfo();
                                auto status = info.PlaybackStatus();
                                if (status == lastStatus) return;
                                lastStatus = status;
                                if (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
                                    PlaybackUpdateEvent("playback-update"_spr, true).post();
                                } else {
                                    PlaybackUpdateEvent("playback-update"_spr, false).post();
                                }
                                
                            });
                        }
                    );
                }
                return true;
            } catch (const winrt::hresult_error & e) {
                log::debug("Failed to get media session: {}", winrt::to_string(e.message()));
                return false;
            } catch (...) {
                log::debug("Unknown exception while retrieving media session.");
                return false;
            }
            break;
        }
        
        if (status == AsyncStatus::Error || status == AsyncStatus::Canceled) {
            log::debug("Failed to retrieve media manager.");
            return false;
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(3)) {
            log::debug("Media Manager request timed out. Media controls unavailable!");
            async.Cancel();
            return false;
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
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        bool result = session.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ? true : false;
        return result;
    } catch (const hresult_error & e) {
        log::debug("Failed to get media session or playback info: {}", to_string(e.message().c_str()));
        return false;
    }
}

bool PlaybackManager::control(bool play) {
    if (m_wine || !m_mediaManager || m_immune) return false;
    try {
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        play ? session.TryPlayAsync() : session.TryPauseAsync();
        return true;
    } catch (...) {
        log::debug("Playback control error!");
        return false;
    }
}

bool PlaybackManager::skip(bool direction) {
    if (m_wine || !m_mediaManager) return false;
    try {
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        direction ? session.TrySkipNextAsync() : session.TrySkipPreviousAsync();
        return true;
    } catch (...) {
        log::debug("Playback control error!");
        return false;
    }
}

bool PlaybackManager::toggleControl() {
    if (m_wine || !m_mediaManager) return false;
    try {
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        session.TryTogglePlayPauseAsync();
        return true;
    } catch (...) {
        log::debug("Playback control error!");
        return false;
    }
}

std::optional<std::string> PlaybackManager::getCurrentSongTitle() {
    if (m_wine) return std::nullopt;

    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (!currentSession) return std::nullopt;

        auto propAsync = currentSession.TryGetMediaPropertiesAsync();
        auto props = propAsync.get(); // blocks until ready

        return winrt::to_string(props.Title());
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get song title: {}", winrt::to_string(e.message()));
    }

    return std::nullopt;
}

std::optional<std::string> PlaybackManager::getCurrentSongArtist() {
    if (m_wine) return std::nullopt;

    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (!currentSession) return std::nullopt;

        auto propAsync = currentSession.TryGetMediaPropertiesAsync();
        auto props = propAsync.get(); // blocks until ready

        return winrt::to_string(props.Artist());
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get song artist: {}", winrt::to_string(e.message()));
    }

    return std::nullopt;
}