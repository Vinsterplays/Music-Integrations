#include "PlaybackManager.hpp"
#include "Geode/loader/Event.hpp"
#include "Geode/loader/Log.hpp"
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

    auto attachToSession = [this] {
        if (!m_mediaManager) return;
        if (auto session = m_mediaManager.GetCurrentSession()) {
            static std::string lastSongName, lastArtistName;
            static GlobalSystemMediaTransportControlsSessionPlaybackStatus lastStatus{};
            session.MediaPropertiesChanged([this](auto session, auto) {
                auto op = session.TryGetMediaPropertiesAsync();
                op.Completed([this, session](auto const& async2, auto) {
                    if (async2.Status() != AsyncStatus::Completed) return;

                    Loader::get()->queueInMainThread([this, session, async2] {
                        auto props = async2.GetResults();
                        auto title  = winrt::to_string(props.Title());
                        auto artist = winrt::to_string(props.Artist());
                        if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr,  title).post(); }
                        if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr, artist).post(); }
                    });
                });
            });

            session.PlaybackInfoChanged([this](auto s, auto) {
                if (!s || s != m_mediaManager.GetCurrentSession()) return;
                Loader::get()->queueInMainThread([this, s] {
                    auto status = s.GetPlaybackInfo().PlaybackStatus();
                    if (status == lastStatus) return;
                    lastStatus = status;
                    PlaybackUpdateEvent("playback-update"_spr,
                        status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing).post();
                });
            });

            auto op = session.TryGetMediaPropertiesAsync();
                op.Completed([this, session](auto const& async2, auto) {
                    if (async2.Status() != AsyncStatus::Completed) return;

                    Loader::get()->queueInMainThread([this, session, async2] {
                        auto props = async2.GetResults();
                        auto title  = winrt::to_string(props.Title());
                        auto artist = winrt::to_string(props.Artist());
                        if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr,  title).post(); }
                        if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr, artist).post(); }
                    });
                });
            Loader::get()->queueInMainThread([this, session] {
                auto status = session.GetPlaybackInfo().PlaybackStatus();
                if (status == lastStatus) return;
                lastStatus = status;
                PlaybackUpdateEvent("playback-update"_spr,
                    status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing).post();
            });
        }
    };

    while (true) {
        auto status = async.Status();

        if (status == AsyncStatus::Completed) {
            try {
                m_mediaManager = async.GetResults();
                m_mediaManager.CurrentSessionChanged(
                    [this, attachToSession](auto, auto) {
                        attachToSession();
                    }
                );

                attachToSession();
                
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
    auto session = m_mediaManager.GetCurrentSession();
    if (!session) return false;
    bool result = session.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ? true : false;
    return result;
}

bool PlaybackManager::control(bool play) {
    if (m_wine || !m_mediaManager || m_immune) return false;
    auto session = m_mediaManager.GetCurrentSession();
    if (!session) return false;
    play ? session.TryPlayAsync() : session.TryPauseAsync();
    return true;
}

bool PlaybackManager::skip(bool direction) {
    if (m_wine || !m_mediaManager) return false;
    auto session = m_mediaManager.GetCurrentSession();
    if (!session) return false;
    direction ? session.TrySkipNextAsync() : session.TrySkipPreviousAsync();
    return true;
}

bool PlaybackManager::toggleControl() {
    if (m_wine || !m_mediaManager) return false;
    auto session = m_mediaManager.GetCurrentSession();
    if (!session) return false;
    session.TryTogglePlayPauseAsync();
    return true;
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