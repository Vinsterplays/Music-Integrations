#include "httpManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <arc/time/Sleep.hpp>

using namespace geode::prelude;

SpotifyAuth* SpotifyAuth::instance = nullptr;

void SpotifyAuth::start(std::function<void(std::string)> callback) {
    m_callback = callback;
    
    m_timer = async::spawn(
        arc::sleep(asp::Duration::fromSecs(120)),
        [this] { this->stop(); }
    );

    m_server.start([this](std::string code) {
        queueInMainThread([this, code] {
            Mod::get()->setSavedValue("spotify-code", code);
            
            this->stop();
            m_timer.abort();
            
            if (m_callback) {
                m_callback(code);
            }
        });
    });
}

void SpotifyAuth::stop() {
    m_server.stop();
}

void SpotifyAuth::failRefresh(std::string const& refreshToken, bool badToken) {
    m_refreshing = false;
    m_refreshCallbacks.clear();
    m_lastFailedRefresh = std::chrono::steady_clock::now();
    if (badToken) m_badRefreshTokens.insert(refreshToken);
}

void SpotifyAuth::refresh(std::function<void(std::string)> callback) {
    if (callback) m_refreshCallbacks.push_back(std::move(callback));
    if (m_refreshing) return;

    auto refreshToken = Mod::get()->getSavedValue<std::string>("spotify-refresh", "");
    if (std::chrono::steady_clock::now() - m_lastFailedRefresh < REFRESH_COOLDOWN || m_badRefreshTokens.contains(refreshToken)) {
        log::debug("Skipping token refresh, last attempt failed");
        m_refreshCallbacks.clear();
        return;
    }

    m_refreshing = true;
    auto req = web::WebRequest();
    m_listener.spawn(
        req.post("https://craftify.thatgravyboat.tech/api/v1/public/auth?type=refresh&code=" + refreshToken, geode::getMod()),
        [this, refreshToken](web::WebResponse value) {
            auto jsonResult = value.json();
            if (!jsonResult.isOk()) {
                log::error("Failed to parse JSON response");
                failRefresh(refreshToken, false);
                return;
            }
            
            auto jsonUnwrap = jsonResult.unwrap();
            if (!jsonUnwrap.contains("access_token")) {
                failRefresh(refreshToken, value.code() != 429 && value.code() < 502);
                 if (value.code() == 500) {
                    log::debug("Authorization revoked");
                    Mod::get()->setSavedValue<bool>("hasAuthorized", false);
                    return;
                }
                log::error("No access_token in response!");
                log::error("Code: {}, Body: {}", value.code(), value.string());
                return;
            }
            
            auto tokenResult = jsonUnwrap["access_token"].asString();
            if (!tokenResult.isOk()) {
                log::error("Failed to get access_token as string");
                log::error("Code: {}, Body: {}", value.code(), value.string());
                failRefresh(refreshToken, false);
                return;
            }

            auto refreshResult = jsonUnwrap["refresh_token"].asString();
            if (refreshResult && refreshResult.isOk()) {
                std::string refresh = refreshResult.unwrap();
                Mod::get()->setSavedValue("spotify-refresh", refresh);
            } else {
                log::debug("No refresh token in response, not updating stored refresh token");
            }
            
            std::string token = tokenResult.unwrap();
            Mod::get()->setSavedValue("spotify-token", token);
            
            m_refreshing = false;
            auto callbacks = std::move(m_refreshCallbacks);
            m_refreshCallbacks.clear();
            for (auto& cb : callbacks) {
                cb(token);
            }
        }
    );
}