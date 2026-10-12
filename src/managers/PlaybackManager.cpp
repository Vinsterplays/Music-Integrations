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
    #else
    return false;
    #endif

}

#ifdef GEODE_IS_WINDOWS
static std::optional<GlobalSystemMediaTransportControlsSessionPlaybackStatus> getPlaybackStatus(GlobalSystemMediaTransportControlsSession const& session) {
    if (!session) return std::nullopt;
    try {
        auto info = session.GetPlaybackInfo();
        if (!info) return std::nullopt;
        return info.PlaybackStatus();
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get playback info: {}", winrt::to_string(e.message()));
    }
    return std::nullopt;
}

bool PlaybackManager::getMediaManager() {
    if (!isWindows()) {
        log::debug("Running on non-Windows system, skipping media manager initialization.");
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

                    auto props = async2.GetResults();
                    if (!props) return;
                    auto title  = winrt::to_string(props.Title());
                    auto artist = winrt::to_string(props.Artist());
                    auto thumbnail = props.Thumbnail();

                    Loader::get()->queueInMainThread([this, title, artist] {
                        if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr).send(title); }
                        if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr).send(artist); }
                    });

                    if (!thumbnail) return;
                    auto streamOp = thumbnail.OpenReadAsync();
                    streamOp.Completed([](auto const& streamAsync, auto) {
                        if (streamAsync.Status() != AsyncStatus::Completed) return;

                        auto stream = streamAsync.GetResults();
                        uint32_t size = static_cast<uint32_t>(stream.Size());
                        if (size == 0) return;

                        winrt::Windows::Storage::Streams::DataReader reader(stream);
                        auto loadOp = reader.LoadAsync(size);
                        loadOp.Completed([reader, size](auto const& loadAsync, auto) mutable {
                            if (loadAsync.Status() != AsyncStatus::Completed) return;

                            std::vector<uint8_t> bytes(size);
                            reader.ReadBytes(bytes);

                            Loader::get()->queueInMainThread([bytes = std::move(bytes)] {
                                ThumbnailUpdateEvent("image-update"_spr).send(bytes);
                            });
                        });
                    });
                });
            });

            session.PlaybackInfoChanged([this](auto s, auto) {
                if (!s || !m_mediaManager || s != m_mediaManager.GetCurrentSession()) return;
                Loader::get()->queueInMainThread([this, s] {
                    auto playbackStatus = getPlaybackStatus(s);
                    if (!playbackStatus) return;
                    auto status = *playbackStatus;
                    if (status == lastStatus) return;
                    lastStatus = status;
                    PlaybackUpdateEvent("playback-update"_spr).send(status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                });
            });

            // Initial fetch
            auto op = session.TryGetMediaPropertiesAsync();
            op.Completed([this, session](auto const& async2, auto) {
                if (async2.Status() != AsyncStatus::Completed) return;

                auto props = async2.GetResults();
                if (!props) return;
                auto title  = winrt::to_string(props.Title());
                auto artist = winrt::to_string(props.Artist());
                auto thumbnail = props.Thumbnail();

                Loader::get()->queueInMainThread([this, title, artist] {
                    if (title  != lastSongName)  { lastSongName  = title;  SongUpdateEvent("title-update"_spr).send(title); }
                    if (artist != lastArtistName) { lastArtistName = artist; SongUpdateEvent("artist-update"_spr).send(artist); }
                });

                if (!thumbnail) return;
                auto streamOp = thumbnail.OpenReadAsync();
                streamOp.Completed([](auto const& streamAsync, auto) {
                    if (streamAsync.Status() != AsyncStatus::Completed) return;

                    auto stream = streamAsync.GetResults();
                    uint32_t size = static_cast<uint32_t>(stream.Size());
                    if (size == 0) return;

                    winrt::Windows::Storage::Streams::DataReader reader(stream);
                    auto loadOp = reader.LoadAsync(size);
                    loadOp.Completed([reader, size](auto const& loadAsync, auto) mutable {
                        if (loadAsync.Status() != AsyncStatus::Completed) return;

                        std::vector<uint8_t> bytes(size);
                        reader.ReadBytes(bytes);

                        Loader::get()->queueInMainThread([bytes = std::move(bytes)] {
                            ThumbnailUpdateEvent("image-update"_spr).send(bytes);
                        });
                    });
                });
            });

            Loader::get()->queueInMainThread([this, session] {
                auto playbackStatus = getPlaybackStatus(session);
                if (!playbackStatus) return;
                auto status = *playbackStatus;
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
            } catch (const winrt::hresult_error& e) {
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
#endif

void PlaybackManager::isPlaybackActive(std::function<void(bool)> callback) {
    if (!isWindows()) {
        if (!Mod::get()->getSavedValue<bool>("hasAuthorized")) {callback(false); return; }

        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) { callback(false); return; }
        spotifyisPlaybackActive(token, [callback](bool isPlaying) { callback(isPlaying); });
    } else {
        #ifdef GEODE_IS_WINDOWS
        if (!m_mediaManager) return callback(false);
        auto status = getPlaybackStatus(m_mediaManager.GetCurrentSession());
        callback(status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
        #else
        callback(false);
        #endif
    }
}

bool PlaybackManager::control(bool play) {
    if (!isWindows()) {
        if (!Mod::get()->getSavedValue<bool>("hasAuthorized")) return false;

        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) { return false; }
        PlaybackManager::get().spotifyControlRequest(token, 0, play);
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
     if (!isWindows()) {
        if (!Mod::get()->getSavedValue<bool>("hasAuthorized")) return false;

        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) { return false; }
        if (!spotifyRegisterControl()) return false;
        PlaybackManager::get().spotifySkipRequest(token, 0, direction);
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
    if (!isWindows()) {
        if (!Mod::get()->getSavedValue<bool>("hasAuthorized")) return false;

        std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
        if (token.empty()) { return false; }
        if (!spotifyRegisterControl()) return false;
        spotifyisPlaybackActive(token, [](bool isPlaying) {
            std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
            if (token.empty()) return; 
            PlaybackManager::get().spotifyControlRequest(token, 0, !isPlaying, false);
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
    if (!m_mediaManager) return std::nullopt;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (!currentSession) return std::nullopt;

        auto propAsync = currentSession.TryGetMediaPropertiesAsync();
        auto props = propAsync.get(); // blocks until ready
        if (!props) return std::nullopt;

        return winrt::to_string(props.Title());
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get song title: {}", winrt::to_string(e.message()));
    }

    return std::nullopt;
}

std::optional<std::string> PlaybackManager::getCurrentSongArtist() {
    if (!m_mediaManager) return std::nullopt;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (!currentSession) return std::nullopt;

        auto propAsync = currentSession.TryGetMediaPropertiesAsync();
        auto props = propAsync.get(); // blocks until ready
        if (!props) return std::nullopt;

        return winrt::to_string(props.Artist());
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get song artist: {}", winrt::to_string(e.message()));
    }

    return std::nullopt;
}

void PlaybackManager::getCurrentSongThumbnail(std::function<void(std::vector<uint8_t>)> callback) {
    if (!m_mediaManager) return;
    try {
        auto currentSession = m_mediaManager.GetCurrentSession();
        if (!currentSession) return;

        auto propAsync = currentSession.TryGetMediaPropertiesAsync();
        propAsync.Completed([callback](auto const& async, auto) {
            if (async.Status() != AsyncStatus::Completed) { Loader::get()->queueInMainThread([callback] { callback({}); }); return; };

            auto props = async.GetResults();
            auto thumbnail = props ? props.Thumbnail() : nullptr;
            if (!thumbnail) { Loader::get()->queueInMainThread([callback] { callback({}); }); return; };

            auto streamOp = thumbnail.OpenReadAsync();
            streamOp.Completed([callback](auto const& streamAsync, auto) {
                if (streamAsync.Status() != AsyncStatus::Completed) { Loader::get()->queueInMainThread([callback] { callback({}); }); return; };

                auto stream = streamAsync.GetResults();
                uint32_t size = static_cast<uint32_t>(stream.Size());
                if (size == 0) { Loader::get()->queueInMainThread([callback] { callback({}); }); return; }

                winrt::Windows::Storage::Streams::DataReader reader(stream);
                auto loadOp = reader.LoadAsync(size);
                loadOp.Completed([reader, size, callback](auto const& loadAsync, auto) mutable {
                    if (loadAsync.Status() != AsyncStatus::Completed) { Loader::get()->queueInMainThread([callback] { callback({}); }); return; }

                    std::vector<uint8_t> bytes(size);
                    reader.ReadBytes(bytes);

                    Loader::get()->queueInMainThread([bytes = std::move(bytes), callback] {
                        callback(bytes);
                    });
                });
            });
        });
    } catch (const winrt::hresult_error& e) {
        log::debug("Failed to get song thumbnail: {}", winrt::to_string(e.message()));
    }
}
#endif

bool PlaybackManager::spotifyCanRequest(bool isPoll) {
    if (std::chrono::steady_clock::now() < m_retryAfter) return false;
    if (isPoll && m_spotifyStopped) return false;
    return true;
}

void PlaybackManager::spotifyOnSuccess() {
    m_spotifyErrorCount = 0;
}

void PlaybackManager::spotifyOnError(std::string_view reason) {
    if (m_spotifyStopped) return;
    m_spotifyErrorCount += 1;
    if (m_spotifyErrorCount < m_maxErrorLimit) {
        log::warn("Spotify request failed ({}/{}): {}", m_spotifyErrorCount, m_maxErrorLimit, reason);
        return;
    }
    m_spotifyStopped = true;
    log::error("Too many Spotify errors in a row, polling stopped. Last error: {}", reason);
}

bool PlaybackManager::spotifyRegisterControl() {
    auto now = std::chrono::steady_clock::now();
    auto drained = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastControlDecay);
    if (drained.count() > 0) {
        m_controlSpamCount = std::max<int>(0, m_controlSpamCount - static_cast<int>(drained.count()));
        m_lastControlDecay += drained;
    }

    if (m_controlSpamBlocked) {
        if (m_controlSpamCount > 0) {
            log::debug("rate limited!");
            return false;
        }
        log::debug("No longer rate limited!");
        m_controlSpamBlocked = false;
    }

    m_controlSpamCount += 1;
    if (m_controlSpamCount >= m_controlSpamLimit) {
        log::debug("Rate limit hit! {}", m_controlSpamCount);
        m_controlSpamBlocked = true;
        RateLimitUpdate("rate-limit-update"_spr).send();
    }
    return true;
}

bool PlaybackManager::spotifyHandleRateLimit(web::WebResponse const& value) {
    if (value.code() != 429) return false;

    std::chrono::seconds wait = DEFAULT_RETRY_AFTER;
    if (auto header = value.header("Retry-After")) {
        wait = std::chrono::seconds(numFromString<int>(*header).unwrapOr(static_cast<int>(DEFAULT_RETRY_AFTER.count())));
    }
    m_retryAfter = std::chrono::steady_clock::now() + wait;
    log::warn("Spotify rate limited, pausing requests for {}s", wait.count());
    spotifyOnError("Rate limited (429)");
    return true;
}

void PlaybackManager::pollSpotify(bool force) {
    if (!force && !m_spotifyPolling) return;
    if (isWindows() || !Mod::get()->getSavedValue<bool>("hasAuthorized")) return;
    if (!spotifyCanRequest(true)) return;

    auto now = std::chrono::steady_clock::now();
    if (now - m_lastPoll < POLL_INTERVAL) return;

    std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
    if (token.empty()) {
        log::error("No Spotify token available");
        return;
    }

    m_lastPoll = now;
    spotifyGetPlaybackInfo(token, 0);
}

void PlaybackManager::resumeSpotify() {
    if (!m_spotifyStopped) return;
    log::debug("Resuming Spotify polling");
    m_spotifyStopped = false;
    m_spotifyErrorCount = 0;
}

void PlaybackManager::spotifyControlRequest(std::string token, int retryCount, bool play, bool checkState) {
    if (retryCount > 1 || !spotifyCanRequest(false)) {
        log::error("Max retries reached for control request");
        return;
    }

    if (checkState) {
        spotifyisPlaybackActive(token, [this, play](bool isPlaying) {
            if (isPlaying == play) {
                log::debug("Playback already {}, skipping request", play ? "playing" : "paused");
                PlaybackUpdateEvent("playback-update"_spr).send(play);
                return;
            }
            std::string token = Mod::get()->getSavedValue<std::string>("spotify-token", "");
            if (token.empty()) return;
            spotifyControlRequest(token, 0, play, false);
        });
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    req.header("Content-Length", "0");
    req.bodyString("");

    m_controlListener.spawn(
        req.put(
            play ? "https://api.spotify.com/v1/me/player/play"
                : "https://api.spotify.com/v1/me/player/pause",
            geode::getMod()
        ),
        [this, retryCount, play, token](web::WebResponse value) {
            auto code = value.code();

            if (spotifyHandleRateLimit(value)) return;

            if (code == 401) {
                spotifyOnError("Unauthorized");
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, play](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifyControlRequest(newToken, retryCount + 1, play, false);
                });
                return;
            }
            
            if (code == 500) { Mod::get()->setSavedValue<bool>("hasAuthorized", false); return; }
            if (code == 404) { log::debug("No active device"); return; }
            
            if (value.ok()) {
                spotifyOnSuccess();
                PlaybackUpdateEvent("playback-update"_spr).send(play);
                log::debug("Successfully {} playback", play ? "started" : "paused");
                return;
            }
            
            spotifyOnError(fmt::format("HTTP {}", code));
            log::error("Request failed with code: {} {}", code, value.string());
        }
    );
}

void PlaybackManager::spotifyisPlaybackActive(std::string token, std::function<void(bool)> callback, int retryCount) {
    if (retryCount > 1 || !spotifyCanRequest(false)) {
        log::error("Max retries reached for playback info request");
        return callback(false);
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    
    m_controlListener.spawn(
        req.get("https://api.spotify.com/v1/me/player"),
        [this, retryCount, callback, token](web::WebResponse value) {
            auto code = value.code();
            
            if (spotifyHandleRateLimit(value)) return;

            if (code == 401) {
                spotifyOnError("Unauthorized");
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, callback](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifyisPlaybackActive(newToken, callback, retryCount + 1);
                });
                return;
            }
            
            if (code == 500) { Mod::get()->setSavedValue<bool>("hasAuthorized", false); return; }
            if (code == 204) { log::debug("No active device"); return; }
            
            if (value.ok()) {
                spotifyOnSuccess();
                auto jsonResult = value.json();
                if (!jsonResult.isOk()) {
                    log::error("Failed to parse JSON response");
                    log::error("Code: {}, Body: {}", code, value.string());
                    callback(false);
                    return;
                }
                auto jsonUnwrap = jsonResult.unwrap();
                if (!jsonUnwrap.contains("is_playing")) {
                    log::error("No is_playing field in response!");
                    log::error("Code: {}, Body: {}", code, value.string());
                    callback(false);
                    return;
                }
                callback(jsonUnwrap["is_playing"].asBool().unwrap());
                return;
            }
            
            spotifyOnError(fmt::format("HTTP {}", code));
            log::error("Request failed with code: {} {}", code, value.string());
            callback(false);
        }
    );
}

void PlaybackManager::spotifySkipRequest(std::string token, int retryCount, bool direction) {
    if (retryCount > 1 || !spotifyCanRequest(false)) {
        log::error("Max retries reached for control request");
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    req.header("Content-Length", "0");
    req.bodyString("");
    
    m_controlListener.spawn(
        req.post(
            direction ? "https://api.spotify.com/v1/me/player/next"
                : "https://api.spotify.com/v1/me/player/previous",
            geode::getMod()
        ),
        [this, retryCount, direction, token](web::WebResponse value) {
            auto code = value.code();
            
            if (spotifyHandleRateLimit(value)) return;

            if (code == 401) {
                spotifyOnError("Unauthorized");
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount, direction](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifySkipRequest(newToken, retryCount + 1, direction);
                });
                return;
            }
            
            if (code == 500) { Mod::get()->setSavedValue<bool>("hasAuthorized", false); return; }
            if (code == 404) { log::debug("No active device"); return; }
            
            if (value.ok()) {
                spotifyOnSuccess();
                log::debug("Successfully skipped track: {}", direction ? "next" : "previous");
                return;
            }
            
            spotifyOnError(fmt::format("HTTP {}", code));
            log::error("Request failed with code: {} {}", code, value.string());
        }
    );
}

void PlaybackManager::spotifyGetPlaybackInfo(std::string token, int retryCount) {
    if (retryCount > 1 || !spotifyCanRequest(true)) {
        log::error("Max retries reached for playback info request");
        return;
    }
    
    auto req = web::WebRequest();
    req.header("Authorization", fmt::format("Bearer {}", token));
    
    m_pollListener.spawn(
        req.get("https://api.spotify.com/v1/me/player/currently-playing", geode::getMod()),
        [this, retryCount, token](web::WebResponse value) {
            auto code = value.code();
            
            if (spotifyHandleRateLimit(value)) return;

            if (code == 401) {
                spotifyOnError("Unauthorized");
                log::debug("Token expired, refreshing... (attempt {})", retryCount + 1);
                SpotifyAuth::get()->refresh([this, retryCount](std::string newToken) {
                    log::debug("Token refreshed, retrying request");
                    spotifyGetPlaybackInfo(newToken, retryCount + 1);
                });
                return;
            }
            
            if (code == 500) { Mod::get()->setSavedValue<bool>("hasAuthorized", false); return; }
            if (code == 204) { log::debug("No active device"); return; }
            
            if (!value.ok()) {
                spotifyOnError(fmt::format("HTTP {}", code));
                log::error("Playback request failed: {} {}", code, value.string());
                return;
            }
            
            auto jsonResult = value.json();
            if (!jsonResult.isOk()) {
                spotifyOnError("Invalid playback JSON");
                log::error("Failed to parse playback JSON");
                log::error("Code: {}, Body: {}", code, value.string());
                return;
            }
            
            spotifyOnSuccess();
            auto root = jsonResult.unwrap();
            if (!root.contains("currently_playing_type") ||
                root["currently_playing_type"].asString().unwrapOr("") != "track") {
                log::debug("Nothing playable right now");
                return;
            }
            
            if (root.contains("is_playing")) {
                PlaybackUpdateEvent("playback-update"_spr).send(root["is_playing"].asBool().unwrapOr(false));
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
            
            auto title = titleOpt.unwrap();
            auto artist = artistOpt.unwrap();
            auto thumbnail = coverOpt.unwrap();
            Loader::get()->queueInMainThread([title, artist, thumbnail] {
                SongUpdateEvent("title-update"_spr).send(title);
                SongUpdateEvent("artist-update"_spr).send(artist);
                SongUpdateEvent("image-update"_spr).send(thumbnail);
            });
        }
    );
}
