#pragma once
#include "../server/server.h"
#include <Geode/utils/web.hpp>
#include <chrono>
#include <unordered_set>

using namespace geode::prelude;

class SpotifyAuth {
protected:
    static SpotifyAuth* instance;
    LocalAuthServer m_server;
    std::function<void(std::string)> m_callback;
    async::TaskHolder<web::WebResponse> m_listener;
    arc::TaskHandle<void> m_timer;

    std::chrono::seconds REFRESH_COOLDOWN = std::chrono::seconds(10);
    bool m_refreshing = false;
    std::vector<std::function<void(std::string)>> m_refreshCallbacks;
    std::chrono::steady_clock::time_point m_lastFailedRefresh{};
    std::unordered_set<std::string> m_badRefreshTokens;
    void failRefresh(std::string const& refreshToken, bool badToken);

public:
    void start(std::function<void(std::string)> callback);
    void stop();
    void refresh(std::function<void(std::string)> callback);
    
    static SpotifyAuth* get() {
        if (!instance) {
            instance = new SpotifyAuth();
        }
        return instance;
    }
};