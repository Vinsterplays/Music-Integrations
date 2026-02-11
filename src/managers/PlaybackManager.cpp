#include "PlaybackManager.hpp"
#include "Geode/loader/Log.hpp"
#include "MusicOverlayManager.cpp"
#include "httpManager.hpp"

bool PlaybackManager::isWindows() {
    #ifdef GEODE_IS_WINDOWS
    static bool wine = [] -> bool {
        HMODULE hModule = GetModuleHandleA("ntdll.dll");
        if (!hModule) return false;
        FARPROC func = GetProcAddress(hModule, "wine_get_version");
        if (!func) return false;
        return true;
    }();
    return !wine;
    log::debug("Running on Windows: {}", !wine);
    #else
    return false;
    log::debug("Running on Windows: {}", false);
    #endif

}

bool PlaybackManager::getMediaManager() {
    if (!isWindows()) {
        log::debug("Running on non-Windows system, skipping media manager initialization.");
        return false;
    }
    #ifdef GEODE_IS_WINDOWS

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
                        if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr).send(title); }
                        if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr).send(artist); }
                    });
                });
            });

            session.PlaybackInfoChanged([this](auto s, auto) {
                if (!s || s != m_mediaManager.GetCurrentSession()) return;
                Loader::get()->queueInMainThread([this, s] {
                    auto status = s.GetPlaybackInfo().PlaybackStatus();
                    if (status == lastStatus) return;
                    lastStatus = status;
                    PlaybackUpdateEvent("playback-update"_spr).send(status ==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                });
            });

            auto op = session.TryGetMediaPropertiesAsync();
                op.Completed([this, session](auto const& async2, auto) {
                    if (async2.Status() != AsyncStatus::Completed) return;

                    Loader::get()->queueInMainThread([this, session, async2] {
                        auto props = async2.GetResults();
                        auto title  = winrt::to_string(props.Title());
                        auto artist = winrt::to_string(props.Artist());
                        if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr).send(title); }
                        if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr).send(artist); }
                    });
                });
            Loader::get()->queueInMainThread([this, session] {
                auto status = session.GetPlaybackInfo().PlaybackStatus();
                if (status == lastStatus) return;
                lastStatus = status;
                PlaybackUpdateEvent("playback-update"_spr).send(status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
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
    #endif
}

#ifdef GEODE_IS_WINDOWS
void PlaybackManager::removeMediaManager() {
    m_mediaManager = nullptr;
}
#endif

void PlaybackManager::isPlaybackActive(std::function<void(bool)> callback) {
    if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !isWindows()) {
        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) {
            log::error("No Spotify token available");
            callback(false);
            return;
        }
        spotifyisPlaybackActive(token, [callback](bool isPlaying) {
            callback(isPlaying);
        });
    } else {
        #ifdef GEODE_IS_WINDOWS
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) callback(false);
        bool result = session.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ? true : false;
        callback(result);
        #else
        callback(false);
        #endif
    }
}

bool PlaybackManager::control(bool play) {
    if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !isWindows()) {
        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) {
            log::error("No Spotify token available");
            return false;
        }
        PlaybackManager::get().spotifyControlRequest(Mod::get()->getSavedValue<std::string>("spotify-token", ""), 0, play);
        return true;
    } else {
        #ifdef GEODE_IS_WINDOWS
        if (!m_mediaManager || m_immune) return false;
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        play ? session.TryPlayAsync() : session.TryPauseAsync();
        #endif
        return true;
    }
}

bool PlaybackManager::skip(bool direction) {
     if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !isWindows()) {
        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) {
            log::error("No Spotify token available");
            return false;
        }
        PlaybackManager::get().spotifySkipRequest(Mod::get()->getSavedValue<std::string>("spotify-token", ""), 0, direction);
        return true;
     } else {
        #ifdef GEODE_IS_WINDOWS
        if (!m_mediaManager) return false;
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        direction ? session.TrySkipNextAsync() : session.TrySkipPreviousAsync();
        #endif
        return true;
     }
}

bool PlaybackManager::toggleControl() {
    if (Mod::get()->getSavedValue<bool>("hasAuthorized") && !isWindows()) {
        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) {
            log::error("No Spotify token available");
            return false;
        }
        spotifyisPlaybackActive(token, [](bool isPlaying) {
                PlaybackManager::get().spotifyControlRequest(Mod::get()->getSavedValue<std::string>("spotify-token", ""), 0, !isPlaying);
        });
        return true;
    } else {
        #ifdef GEODE_IS_WINDOWS
        if (!m_mediaManager) return false;
        auto session = m_mediaManager.GetCurrentSession();
        if (!session) return false;
        session.TryTogglePlayPauseAsync();
        #endif
        return true;
    }
}

#ifdef GEODE_IS_WINDOWS

std::optional<std::string> PlaybackManager::getCurrentSongTitle() {
    if (!isWindows()) return std::nullopt;

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
    if (!isWindows()) return std::nullopt;

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

#endif

void PlaybackManager::spotifyControlRequest(std::string token, int retryCount, bool play) {
    if (retryCount > 1) {
        log::error("Max retries reached for control request");
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    req.header("Content-Length", "0");
    req.bodyString("");
    
    m_listener.spawn(
        req.put(play ? "https://api.spotify.com/v1/me/player/play" : "https://api.spotify.com/v1/me/player/pause", geode::getMod()),
        [this, retryCount, play, token](web::WebResponse value) {
            RateLimitUpdate("rate-limit-update"_spr).send();
            if (value.code() == 401) {
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, play](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifyControlRequest(newToken, retryCount + 1, play);
                });
            } else if (value.code() == 500) {
                Mod::get()->setSavedValue<bool>("hasAuthorized", false);
            } else if (value.code() == 404) {
                log::debug("No active device");
            } else if (value.ok()) {
                log::debug("Successfully {} playback", play ? "started" : "paused");
            } else {
                log::error("Request failed with code: {} {}", value.code(), value.string());
                log::error("Token: {}", token);
            }
        }
    );
}

void PlaybackManager::spotifyisPlaybackActive(std::string token, std::function<void(bool)> callback,int retryCount) {
    if (retryCount > 1) {
        log::error("Max retries reached for playback info request");
        callback(false);
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    
    m_listener.spawn(
        req.get("https://api.spotify.com/v1/me/player"),
        [this, retryCount, callback, token](web::WebResponse value) {
            RateLimitUpdate("rate-limit-update"_spr).send();
            if (value.code() == 401) {
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, callback](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifyisPlaybackActive(newToken, callback, retryCount + 1);
                });
            } else if (value.code() == 500) {
                Mod::get()->setSavedValue<bool>("hasAuthorized", false);
            } else if (value.code() == 204) {
                log::debug("No active device");
            } else if (value.ok()) {
                log::debug("Successfully retrieved playback info");
                auto jsonResult = value.json();
                if (!jsonResult.isOk()) {
                    log::error("Failed to parse JSON response");
                    log::error("Code: {}, Body: {}", value.code(), value.string());
                    callback(false);
                    return;
                }
                auto jsonUnwrap = jsonResult.unwrap();
                if (!jsonUnwrap.contains("is_playing")) {
                    log::error("No is_playing field in response!");
                    log::error("Code: {}, Body: {}", value.code(), value.string());
                    callback(false);
                    return;
                }
                callback(jsonUnwrap["is_playing"].asBool().unwrap());
            } else {
                log::error("Request failed with code: {} {}", value.code(), value.string());
                log::error("Token: {}", token);
                callback(false);
            }
        }
    );
}

void PlaybackManager::spotifySkipRequest(std::string token, int retryCount, bool direction) {
    if (retryCount > 1) {
        log::error("Max retries reached for control request");
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    req.header("Content-Length", "0");
    req.bodyString("");
    
    m_listener.spawn(
        req.post(direction ? "https://api.spotify.com/v1/me/player/next" : "https://api.spotify.com/v1/me/player/previous", geode::getMod()),
        [this, retryCount, direction, token](web::WebResponse value) {
            RateLimitUpdate("rate-limit-update"_spr).send();
            if (value.code() == 401) {
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, direction](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifySkipRequest(newToken, retryCount + 1, direction);
                });
            } else if (value.code() == 500) {
                Mod::get()->setSavedValue<bool>("hasAuthorized", false);
            } else if (value.ok()) {
                log::debug("Successfully skipped track: {}", direction ? "next" : "previous");
            } else if (value.code() == 404) {
                log::debug("No active device");
            } else {
                log::error("Request failed with code: {} {}", value.code(), value.string());
                log::error("Token: {}", token);
            }
        }
    );
}

void PlaybackManager::spotifyGetPlaybackInfo(std::string token, int retryCount) {
    if (retryCount > 1) {
        log::error("Max retries reached for playback info request");
        return;
    }

    web::WebRequest req;
    req.header("Authorization", fmt::format("Bearer {}", token));

    m_listener.spawn(
        req.get("https://api.spotify.com/v1/me/player/currently-playing", geode::getMod()),
        [this, retryCount, token](web::WebResponse value) {
            RateLimitUpdate("rate-limit-update"_spr).send();
            if (value.code() == 401) {
                log::debug("Token expired, refreshing");
                SpotifyAuth::get()->refresh([this, retryCount](std::string newToken) {
                    spotifyGetPlaybackInfo(newToken, retryCount + 1);
                });
                return;
            } else if (value.code() == 500) {
                Mod::get()->setSavedValue<bool>("hasAuthorized", false);
                return;
            } else if (value.code() == 204) {
                log::debug("No active device");
                return;
            } 

            if (!value.ok()) {
                log::error("Playback request failed: {} {}", value.code(), value.string());
                return;
            }

            auto jsonResult = value.json();
            if (!jsonResult.isOk()) {
                log::error("Failed to parse playback JSON");
                log::error("Code: {}, Body: {}", value.code(), value.string());
                return;
            }

            auto root = jsonResult.unwrap();

            if (!root.contains("currently_playing_type") ||
                root["currently_playing_type"].asString().unwrapOr("") != "track") {
                log::debug("Nothing playable right now");
                return;
            }

            if (!root.contains("item")) {
                log::error("Playback response missing item");
                return;
            }

            auto item = root["item"];
            if (item.isNull() || !item.isObject()) {
                log::debug("Item is null, probably an ad or unsupported content");
                return;
            }

            auto titleOpt = item["name"].asString();
            if (!titleOpt.isOk()) {
                log::error("Track missing name");
                return;
            }

            auto artists = item["artists"].asArray();
            if (!artists.isOk() || artists.unwrap().empty()) {
                log::error("Track missing artists");
                return;
            }

            auto artistOpt = artists.unwrap()[0]["name"].asString();
            if (!artistOpt.isOk()) {
                log::error("Artist missing name");
                return;
            }

            auto cover = item["album"]["images"].asArray();
            if (!cover.isOk() || cover.unwrap().empty()) {
                log::error("Track missing cover");
                return;
            }

            auto coverOpt = cover.unwrap()[0]["url"].asString();
            if (!coverOpt.isOk()) {
                log::error("Cover missing URL");
                return;
            }

            SongUpdateEvent("title-update"_spr).send(titleOpt.unwrap());
            SongUpdateEvent("artist-update"_spr).send(artistOpt.unwrap());
            SongUpdateEvent("image-update"_spr).send(coverOpt.unwrap());
        }
    );
}
