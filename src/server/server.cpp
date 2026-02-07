#include "../../include/httplib.h"
#include "server.h"
#include <thread>
#include <atomic>
#include <chrono>

class LocalAuthServer::Impl {
public:
    httplib::Server server;
    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> shouldStop{false};
    std::function<void(std::string)> callback;
    
    ~Impl() {
        shouldStop = true;
        running = false;
        server.stop();
    }
};

void LocalAuthServer::start(std::function<void(std::string)> onToken) {
    if (m_impl) {
        stop();
    }
    
    m_impl = new Impl();
    m_impl->callback = onToken;
    m_impl->running = true;
    m_impl->shouldStop = false;
    
    Impl* implPtr = m_impl;
    
    m_impl->server.Get("/", [implPtr](const httplib::Request &req, httplib::Response &res) {
        if (!implPtr || implPtr->shouldStop) {
            return;
        }
        
        if (req.has_param("code")) {
            std::string code = req.get_param_value("code");
            
            res.set_content(
                "<html><body><h1>Success!</h1><p>You can close this window now.</p></body></html>", 
                "text/html"
            );
            
            if (implPtr->callback) {
                try {
                    implPtr->callback(code);
                } catch (...) {
                    //TODO
                }
            }
            
            implPtr->shouldStop = true;
            std::thread([implPtr]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (implPtr && implPtr->running) {
                    implPtr->server.stop();
                }
            }).detach();
            
        } else {
            res.set_content(
                "<html><body><h1>Error</h1><p>No code received.</p></body></html>", 
                "text/html"
            );
        }
    });
    
    m_impl->thread = std::thread([implPtr]() {
        if (!implPtr) return;
        
        try {
            implPtr->server.listen("127.0.0.1", 21851);
        } catch (...) {
            //TODO
        }
        
        implPtr->running = false;
    });
    
    m_impl->thread.detach();
}

void LocalAuthServer::stop() {
    if (m_impl) {
        m_impl->shouldStop = true;
        m_impl->running = false;
        m_impl->server.stop();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        delete m_impl;
        m_impl = nullptr;
    }
}