#pragma once
#include "Board.hpp"
#include "Connection.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <set>

namespace synccanvas {
struct ServerConfig {
    std::string address = "127.0.0.1";
    unsigned short port = 8765;
    std::filesystem::path dataDirectory = "boards";
    std::string accessKey;
};
class Server {
  public:
    explicit Server(ServerConfig config);
    void run();

  private:
    struct Peer {
        std::shared_ptr<Connection> connection;
        std::string id, name, room;
        bool joined = false;
        std::optional<Shape> preview;
        std::unique_ptr<asio::steady_timer> joinDeadline;
        double tokens = 240;
        std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    };
    struct Room {
        Board board;
        std::set<std::string> peers;
        bool dirty = false;
    };
    ServerConfig config_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    asio::signal_set signals_;
    asio::steady_timer saveTimer_;
    std::map<std::string, Peer> peers_;
    std::map<std::string, Room> rooms_;
    std::uint64_t nextPeer_ = 1;
    void accept();
    void message(const std::string &id, const Json &message);
    void disconnect(const std::string &id);
    void broadcast(const std::string &room, const Json &message);
    void saveDirty();
    void scheduleSave();
    void join(Peer &peer, const Json &message);
    void clearPreview(Peer &peer);
};
} // namespace synccanvas
