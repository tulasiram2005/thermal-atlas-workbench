#pragma once
#include "httplib.h"
#include "core/jobs.hpp"
#include <string>
#include <filesystem>
#include <thread>
namespace wb {
class LocalServer {
public:
    explicit LocalServer(std::filesystem::path assets = {});
    ~LocalServer();
    LocalServer(const LocalServer&) = delete;
    LocalServer& operator=(const LocalServer&) = delete;
    bool start();
    void stop();
    int port() const { return port_; }
    const std::string& token() const { return token_; }
private:
    Jobs jobs_;
    std::atomic<unsigned> streams_{0};
    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
    std::string token_;
};
bool scaffold_smoke(std::chrono::steady_clock::time_point start, double& ready_ms);
}
