#include "synccanvas/NetworkClient.hpp"
namespace synccanvas {
NetworkClient::NetworkClient() : thread_([this] { io_.run(); }) {}
NetworkClient::~NetworkClient() {
    asio::post(io_, [this] {
        stopConnection();
        work_.reset();
    });
    if (thread_.joinable())
        thread_.join();
}
bool NetworkClient::push(Json value) {
    std::lock_guard lock(mutex_);
    const auto bytes = value.dump().size();
    if (events_.size() >= 4096 || eventBytes_ + bytes > 4 * 1024 * 1024)
        return false;
    eventBytes_ += bytes;
    events_.push_back(std::move(value));
    return true;
}
std::vector<Json> NetworkClient::poll() {
    std::lock_guard lock(mutex_);
    std::vector<Json> result;
    result.swap(events_);
    eventBytes_ = 0;
    return result;
}
void NetworkClient::stopConnection() {
    ++generation_;
    resolver_.cancel();
    deadline_.cancel();
    heartbeat_.cancel();
    if (connecting_) {
        asio::error_code ignored;
        connecting_->close(ignored);
        connecting_.reset();
    }
    if (connection_) {
        connection_->close("Disconnected");
        connection_.reset();
    }
}
void NetworkClient::disconnect() {
    asio::post(io_, [this] {
        stopConnection();
        push({{"type", "offline"}, {"message", "Disconnected"}});
    });
}
void NetworkClient::connect(std::string host, std::string port, std::string room, std::string name,
                            std::string accessKey) {
    asio::post(io_, [this, host = std::move(host), port = std::move(port), room = std::move(room),
                     name = std::move(name), accessKey = std::move(accessKey)] {
        stopConnection();
        const auto generation = generation_;
        push({{"type", "connecting"}});
        deadline_.expires_after(std::chrono::seconds(8));
        deadline_.async_wait([this, generation](asio::error_code error) {
            if (!error && generation == generation_) {
                stopConnection();
                push({{"type", "offline"}, {"message", "Connection timed out"}});
            }
        });
        resolver_.async_resolve(
            host, port, [this, generation, room, name, accessKey](asio::error_code error, auto endpoints) {
                if (generation != generation_)
                    return;
                if (error) {
                    stopConnection();
                    push({{"type", "offline"}, {"message", "Host lookup failed"}});
                    return;
                }
                connecting_ = std::make_shared<asio::ip::tcp::socket>(io_);
                auto socket = connecting_;
                asio::async_connect(
                    *socket, endpoints,
                    [this, generation, socket, room, name, accessKey](asio::error_code connectError, auto) {
                        if (generation != generation_)
                            return;
                        if (connectError) {
                            stopConnection();
                            push({{"type", "offline"}, {"message", "Cannot reach server"}});
                            return;
                        }
                        deadline_.cancel();
                        connecting_.reset();
                        connection_ = std::make_shared<Connection>(std::move(*socket));
                        connection_->start(
                            [this, generation](const Json &value) {
                                if (generation != generation_)
                                    return;
                                if (!push(value)) {
                                    connection_->close("UI event queue full");
                                    std::lock_guard lock(mutex_);
                                    events_.clear();
                                    eventBytes_ = 0;
                                    events_.push_back(
                                        {{"type", "offline"}, {"message", "UI fell behind; reconnecting"}});
                                }
                            },
                            [this, generation](const std::string &reason) {
                                if (generation == generation_) {
                                    connection_.reset();
                                    heartbeat_.cancel();
                                    push({{"type", "offline"}, {"message", reason}});
                                }
                            });
                        connection_->send({{"type", "join"},
                                           {"version", protocolVersion},
                                           {"room", room},
                                           {"name", name},
                                           {"access_key", accessKey}});
                        beat(generation);
                    });
            });
    });
}
void NetworkClient::beat(std::size_t generation) {
    heartbeat_.expires_after(std::chrono::seconds(10));
    heartbeat_.async_wait([this, generation](asio::error_code error) {
        if (!error && generation == generation_ && connection_) {
            connection_->send({{"type", "ping"}});
            beat(generation);
        }
    });
}
void NetworkClient::send(Json message) {
    // Rendering thread never touches the socket or the connection's write queue.
    asio::post(io_, [this, message = std::move(message)] {
        if (connection_) {
            try {
                connection_->send(message);
            } catch (const std::exception &) {
                push({{"type", "error"}, {"message", "Drawing is too large"}});
            }
        }
    });
}
} // namespace synccanvas
