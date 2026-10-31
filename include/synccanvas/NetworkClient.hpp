#pragma once
#include "Connection.hpp"
#include <mutex>
#include <thread>

namespace synccanvas {
class NetworkClient {
  public:
    NetworkClient();
    ~NetworkClient();
    NetworkClient(const NetworkClient &) = delete;
    NetworkClient &operator=(const NetworkClient &) = delete;
    void connect(std::string host, std::string port, std::string room, std::string name,
                 std::string accessKey = {});
    void disconnect();
    void send(Json message);
    std::vector<Json> poll();

  private:
    asio::io_context io_;
    asio::executor_work_guard<asio::io_context::executor_type> work_{asio::make_work_guard(io_)};
    asio::ip::tcp::resolver resolver_{io_};
    asio::steady_timer deadline_{io_}, heartbeat_{io_};
    std::shared_ptr<asio::ip::tcp::socket> connecting_;
    std::shared_ptr<Connection> connection_;
    std::thread thread_;
    std::mutex mutex_;
    std::vector<Json> events_;
    std::size_t eventBytes_ = 0;
    std::size_t generation_ = 0;
    bool push(Json value);
    void stopConnection();
    void beat(std::size_t generation);
};
} // namespace synccanvas
