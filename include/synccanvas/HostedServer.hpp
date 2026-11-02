#pragma once
#include "Server.hpp"
#include <atomic>
#include <mutex>
#include <thread>

namespace synccanvas {
// Owns only this application's server; never controls external server processes.
class HostedServer {
  public:
    ~HostedServer();
    void start(ServerConfig config);
    bool stop();
    bool running() const {
        return running_.load();
    }
    std::string error() const;

  private:
    std::unique_ptr<Server> server_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::string error_;
};
} // namespace synccanvas
