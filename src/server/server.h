#pragma once
#include <functional>
#include <string>

class LocalAuthServer {
public:
    void start(std::function<void(std::string)> onToken);
    void stop();
private:
    class Impl;
    Impl* m_impl = nullptr;
};