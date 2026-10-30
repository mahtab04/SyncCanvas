#pragma once
#include "Protocol.hpp"
#include <asio.hpp>
#include <deque>
#include <functional>
#include <memory>

namespace synccanvas {
// All methods and callbacks run on the owning io_context thread.
class Connection : public std::enable_shared_from_this<Connection> {
  public:
    using MessageHandler = std::function<void(const Json &)>;
    using CloseHandler = std::function<void(const std::string &)>;
    explicit Connection(asio::ip::tcp::socket socket);
    void start(MessageHandler message, CloseHandler closed);
    void send(const Json &message);
    void close(const std::string &reason);

  private:
    asio::ip::tcp::socket socket_;
    asio::steady_timer deadline_;
    std::array<std::uint8_t, 4> header_{};
    std::string body_;
    std::deque<std::vector<std::uint8_t>> writes_;
    std::size_t queuedBytes_ = 0;
    bool closed_ = false;
    MessageHandler onMessage_;
    CloseHandler onClose_;
    void readHeader();
    void readBody();
    void write();
    void armDeadline();
};
} // namespace synccanvas
