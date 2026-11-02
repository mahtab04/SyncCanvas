#include "synccanvas/Server.hpp"
#include "synccanvas/DocumentFile.hpp"
#include <algorithm>
#include <csignal>
#include <iostream>

namespace synccanvas {
Server::Server(ServerConfig config)
    : config_(std::move(config)), acceptor_(io_), signals_(io_), saveTimer_(io_) {
    if (config_.handleSignals) {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
    }
    std::filesystem::create_directories(config_.dataDirectory);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(config_.address), config_.port);
    acceptor_.open(endpoint.protocol());
#ifndef _WIN32
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
    acceptor_.bind(endpoint);
    acceptor_.listen(128);
}
void Server::run() {
    if (config_.handleSignals)
        signals_.async_wait([this](asio::error_code error, int) {
            if (!error)
                stop();
        });
    std::cout << "SyncCanvas listening on " << config_.address << ':' << config_.port << std::endl;
    accept();
    scheduleSave();
    io_.run();
}
void Server::stop() {
    asio::post(io_, [this] {
        saveDirty();
        io_.stop();
    });
}
bool Server::hasUnsavedChanges() const {
    return std::any_of(rooms_.begin(), rooms_.end(), [](const auto &room) { return room.second.dirty; });
}
void Server::accept() {
    acceptor_.async_accept([this](asio::error_code error, asio::ip::tcp::socket socket) {
        if (!error) {
            if (peers_.size() >= 128) {
                asio::error_code ignored;
                socket.close(ignored);
            } else {
                auto id = std::to_string(nextPeer_++);
                auto connection = std::make_shared<Connection>(std::move(socket), 512 * 1024);
                Peer peer;
                peer.id = id;
                peer.connection = connection;
                peer.joinDeadline = std::make_unique<asio::steady_timer>(io_);
                peer.joinDeadline->expires_after(std::chrono::seconds(10));
                peer.joinDeadline->async_wait([this, id](asio::error_code deadlineError) {
                    auto it = peers_.find(id);
                    if (!deadlineError && it != peers_.end() && !it->second.joined)
                        it->second.connection->close("Join timed out");
                });
                peers_.emplace(id, std::move(peer));
                connection->start([this, id](const Json &value) { message(id, value); },
                                  [this, id](const std::string &) { disconnect(id); });
            }
        } else if (error != asio::error::operation_aborted)
            std::cerr << "Accept failed: " << error.message() << '\n';
        if (acceptor_.is_open())
            accept();
    });
}
void Server::join(Peer &peer, const Json &value) {
    if (peer.joined)
        throw std::invalid_argument("Already joined");
    if (!config_.accessKey.empty()) {
        const auto key = value.find("access_key");
        if (key == value.end() || !key->is_string() ||
            key->get_ref<const std::string &>() != config_.accessKey)
            throw std::invalid_argument("Access denied. Check the server access key.");
    }
    if (value.at("version") != protocolVersion)
        throw std::invalid_argument("Incompatible protocol version");
    const auto name = value.at("name").get<std::string>();
    const auto roomName = value.at("room").get<std::string>();
    if (!validIdentifier(name, 24) || !validIdentifier(roomName))
        throw std::invalid_argument("Use letters, numbers, hyphens or underscores for name and room");
    if (!rooms_.contains(roomName)) {
        if (rooms_.size() >= 32)
            throw std::length_error("Server room limit reached");
        Room room;
        const auto path = config_.dataDirectory / (roomName + ".json");
        try {
            if (std::filesystem::exists(path))
                room.board.load(readDocumentFile(path));
        } catch (const std::exception &) {
            throw std::runtime_error("Room archive unavailable. Contact the server operator.");
        }
        rooms_.emplace(roomName, std::move(room));
    }
    auto &room = rooms_.at(roomName);
    if (room.peers.size() >= 32)
        throw std::length_error("Room is full");
    peer.name = name;
    peer.room = roomName;
    peer.joined = true;
    peer.joinDeadline->cancel();
    room.peers.insert(peer.id);
    peer.connection->send({{"type", "welcome"},
                           {"version", protocolVersion},
                           {"id", peer.id},
                           {"room", roomName},
                           {"access_controlled", !config_.accessKey.empty()}});
    peer.connection->send({{"type", "snapshot_begin"}, {"revision", room.board.revision()}});
    for (const auto &shape : room.board.shapes())
        peer.connection->send({{"type", "snapshot_shape"}, {"shape", shapeJson(shape)}});
    peer.connection->send({{"type", "snapshot_end"}, {"revision", room.board.revision()}});
    for (const auto &id : room.peers) {
        const auto &member = peers_.at(id);
        peer.connection->send({{"type", "peer_join"}, {"id", id}, {"name", member.name}});
        if (member.preview)
            peer.connection->send({{"type", "preview"}, {"id", id}, {"shape", shapeJson(*member.preview)}});
    }
    broadcast(roomName, {{"type", "peer_join"}, {"id", peer.id}, {"name", name}});
    std::cout << "Joined room=" << roomName << " peer=" << peer.id << std::endl;
}
void Server::message(const std::string &id, const Json &value) {
    auto it = peers_.find(id);
    if (it == peers_.end())
        return;
    auto &peer = it->second;
    const auto now = std::chrono::steady_clock::now();
    peer.tokens = std::min(240.0, peer.tokens + std::chrono::duration<double>(now - peer.last).count() * 120);
    peer.last = now;
    if (peer.tokens < 1) {
        peer.connection->close("Rate limit exceeded");
        return;
    }
    --peer.tokens;
    try {
        const auto type = value.at("type").get<std::string>();
        if (type == "join") {
            join(peer, value);
            return;
        }
        if (!peer.joined)
            throw std::invalid_argument("Join a room first");
        if (type == "ping") {
            peer.connection->send({{"type", "pong"}});
            return;
        }
        auto &room = rooms_.at(peer.room);
        if (type == "draw") {
            clearPreview(peer);
            auto shape = room.board.add(parseShape(value.at("shape")), peer.id);
            room.dirty = true;
            broadcast(peer.room,
                      {{"type", "added"}, {"shape", shapeJson(shape)}, {"revision", room.board.revision()}});
        } else if (type == "preview") {
            const auto &shape = value.at("shape");
            if (shape.is_null())
                clearPreview(peer);
            else {
                peer.preview = parseShape(shape);
                broadcast(peer.room,
                          {{"type", "preview"}, {"id", peer.id}, {"shape", shapeJson(*peer.preview)}});
            }
        } else if (type == "undo") {
            auto removed = room.board.undo(peer.id);
            if (!removed.empty()) {
                room.dirty = true;
                broadcast(peer.room,
                          {{"type", "removed"}, {"id", removed}, {"revision", room.board.revision()}});
            }
        } else if (type == "cursor") {
            const auto point = parsePoint(value.at("point"));
            broadcast(peer.room, {{"type", "cursor"}, {"id", peer.id}, {"point", {point.x, point.y}}});
        } else
            throw std::invalid_argument("Unknown operation");
    } catch (const Json::exception &) {
        peer.connection->send({{"type", "error"}, {"message", "Invalid operation fields"}});
    } catch (const std::exception &error) {
        peer.connection->send({{"type", "error"}, {"message", std::string(error.what()).substr(0, 200)}});
    }
}
void Server::clearPreview(Peer &peer) {
    if (!peer.preview)
        return;
    peer.preview.reset();
    broadcast(peer.room, {{"type", "preview"}, {"id", peer.id}, {"shape", nullptr}});
}
void Server::broadcast(const std::string &roomName, const Json &value) {
    auto it = rooms_.find(roomName);
    if (it == rooms_.end())
        return;
    for (const auto &id : it->second.peers) {
        auto peer = peers_.find(id);
        if (peer != peers_.end())
            peer->second.connection->send(value);
    }
}
void Server::disconnect(const std::string &id) {
    auto it = peers_.find(id);
    if (it == peers_.end())
        return;
    const auto room = it->second.room;
    if (it->second.joined)
        rooms_.at(room).peers.erase(id);
    peers_.erase(it);
    if (!room.empty())
        broadcast(room, {{"type", "peer_leave"}, {"id", id}});
}
void Server::saveDirty() {
    for (auto &[name, room] : rooms_) {
        if (!room.dirty)
            continue;
        try {
            writeDocumentFile(config_.dataDirectory / (name + ".json"), room.board.document());
            room.dirty = false;
        } catch (const std::exception &error) {
            std::cerr << "Persistence failed for room " << name << ": " << error.what() << '\n';
        }
    }
}
void Server::scheduleSave() {
    saveTimer_.expires_after(std::chrono::seconds(2));
    saveTimer_.async_wait([this](asio::error_code error) {
        if (!error) {
            saveDirty();
            scheduleSave();
        }
    });
}
} // namespace synccanvas
