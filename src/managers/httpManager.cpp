#include "httpManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

SpotifyAuth* SpotifyAuth::instance = nullptr;

void SpotifyAuth::start(std::function<void(std::string)> callback) {
    m_callback = callback;
    
    m_server.start([this](std::string code) {
        queueInMainThread([this, code] {
            Mod::get()->setSavedValue("spotify-code", code);
            
            this->stop();
            
            if (m_callback) {
                m_callback(code);
            }
        });
    });
}

void SpotifyAuth::stop() {
    m_server.stop();
}

void SpotifyAuth::refresh(std::function<void(std::string)> callback) {
    auto req = web::WebRequest();
    m_listener.spawn(
        req.post("https://craftify.thatgravyboat.tech/api/v1/public/auth?type=refresh&code=" + Mod::get()->getSavedValue<std::string>("spotify-refresh", ""), geode::getMod()),
        [callback](web::WebResponse value) {
            auto jsonResult = value.json();
            if (!jsonResult.isOk()) {
                log::error("Failed to parse JSON response");
                return;
            }
            
            auto jsonUnwrap = jsonResult.unwrap();
            if (!jsonUnwrap.contains("access_token")) {
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
            
            if (callback) {
                callback(token);
            }
        }
    );
}