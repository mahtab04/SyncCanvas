#include "synccanvas/HostedServer.hpp"
#include <stdexcept>

namespace synccanvas {
HostedServer::~HostedServer() {
    stop();
}
void HostedServer::start(ServerConfig config) {
    if (server_)
        throw std::logic_error("This window already owns a server");
    config.handleSignals = false;
    auto server = std::make_unique<Server>(std::move(config));
    {
        std::lock_guard lock(mutex_);
        error_.clear();
    }
    server_ = std::move(server);
    running_ = true;
    try {
        thread_ = std::thread([this] {
            try {
                server_->run();
            } catch (const std::exception &) {
                std::lock_guard lock(mutex_);
                error_ = "Hosted server failed. Stop it before restarting.";
            }
            running_ = false;
        });
    } catch (...) {
        running_ = false;
        server_.reset();
        throw;
    }
}
bool HostedServer::stop() {
    if (!server_)
        return true;
    server_->stop();
    if (thread_.joinable())
        thread_.join();
    const bool saved = !server_->hasUnsavedChanges();
    server_.reset();
    running_ = false;
    return saved;
}
std::string HostedServer::error() const {
    std::lock_guard lock(mutex_);
    return error_;
}
} // namespace synccanvas
