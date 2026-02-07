#pragma once
#include "../server/server.h"
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

class SpotifyAuth {
protected:
    static SpotifyAuth* instance;
    LocalAuthServer m_server;
    std::function<void(std::string)> m_callback;
    async::TaskHolder<web::WebResponse> m_listener;
    
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