#include "synccanvas/Application.hpp"
#include "synccanvas/DocumentFile.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glad/gl.h>
#include <iostream>
#include <utility>

namespace synccanvas {
namespace {
std::size_t fieldLimit(int index) {
    constexpr std::array<std::size_t, 5> limits{128, 5, 32, 24, 128};
    return limits[static_cast<std::size_t>(index)];
}
bool allowedFieldCharacter(int index, unsigned c) {
    if (index == 1)
        return c >= '0' && c <= '9';
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '_' || (index == 0 && (c == '.' || c == ':'));
}
} // namespace
Application::Application(std::array<std::string, 5> fields) {
    view_.fields = std::move(fields);
}
bool Application::fieldEditable(int index) const {
    return !view_.online && !view_.connecting && !view_.reconnect &&
           !(view_.hosting && (index == 0 || index == 1 || index == 4));
}
void Application::toggleServer() {
    if (view_.hosting) {
        if (!view_.stopHostArmed || glfwGetTime() > stopHostDeadline_) {
            view_.stopHostArmed = true;
            stopHostDeadline_ = glfwGetTime() + 5;
            view_.status = "Stop hosting? Everyone on this server will disconnect. Click Confirm stop.";
            return;
        }
        view_.stopHostArmed = false;
        // Hosting sets and locks the destination to our own loopback listener.
        const bool leavingRoom = view_.online || view_.connecting || view_.reconnect;
        if (leavingRoom)
            action("connect");
        const bool saved = hostedServer_.stop();
        view_.hosting = false;
        view_.hostStatus = "Server stopped";
        view_.status = saved ? "Server stopped. Saved rooms are kept."
                             : "Server stopped, but some room saves failed. Check the data directory.";
        if (leavingRoom)
            offlineReason_ = view_.status;
        return;
    }
    if (view_.online || view_.connecting || view_.reconnect) {
        view_.status = "Disconnect before starting a server on this PC.";
        return;
    }
    try {
        std::size_t used = 0;
        const int port = std::stoi(view_.fields[1], &used);
        if (used != view_.fields[1].size() || port < 1 || port > 65535)
            throw std::invalid_argument("Port must be between 1 and 65535.");
        if (!view_.fields[4].empty() &&
            (view_.fields[4].size() < 16 || !validIdentifier(view_.fields[4], 128)))
            throw std::invalid_argument("Access key: 16-128 letters, numbers, _ or -. Or leave blank.");
        std::filesystem::path root;
#ifdef _WIN32
        if (const char *state = std::getenv("LOCALAPPDATA"); state && *state)
            root = std::filesystem::path(state) / "SyncCanvas";
#else
        if (const char *state = std::getenv("XDG_STATE_HOME"); state && *state)
            root = std::filesystem::path(state) / "synccanvas";
        else if (const char *home = std::getenv("HOME"); home && *home)
            root = std::filesystem::path(home) / ".local/state/synccanvas";
#endif
        if (root.empty() || !root.is_absolute())
            throw std::runtime_error("Cannot find an absolute per-user storage directory for hosting.");
        ServerConfig config;
        config.address = "0.0.0.0";
        config.port = static_cast<unsigned short>(port);
        config.accessKey = view_.fields[4];
        config.dataDirectory = root / "hosted-boards" / std::to_string(port);
        hostedServer_.start(std::move(config));
        view_.hosting = true;
        view_.fields[0] = "127.0.0.1";
        view_.activeField = -1;
        view_.hostStatus = "Hosting on LAN : " + std::to_string(port);
        view_.status = "Server started. Join a room; other PCs use this PC's LAN IP and the same port.";
    } catch (const std::exception &error) {
        view_.status = "Could not start server (check port/storage): " + std::string(error.what());
    }
}
bool Application::pressed(int key) {
    bool down = glfwGetKey(window_, key) == GLFW_PRESS;
    bool edge = keyEvents_[key] || (down && !keys_[key]);
    keyEvents_[key] = false;
    keys_[key] = down;
    return edge;
}
void Application::character(unsigned codepoint) {
    if (view_.editingColor) {
        const bool hex = (codepoint >= '0' && codepoint <= '9') || (codepoint >= 'a' && codepoint <= 'f') ||
                         (codepoint >= 'A' && codepoint <= 'F');
        if (hex) {
            if (view_.colorSelected)
                view_.colorText.clear();
            view_.colorSelected = false;
            if (view_.colorText.size() < 6)
                view_.colorText.push_back(static_cast<char>(codepoint));
        }
        return;
    }
    if (view_.activeField < 0 || !fieldEditable(view_.activeField))
        return;
    auto &field = view_.fields[view_.activeField];
    const auto limit = fieldLimit(view_.activeField);
    if (allowedFieldCharacter(view_.activeField, codepoint)) {
        if (view_.fieldSelected)
            field.clear();
        view_.fieldSelected = false;
        if (field.size() < limit)
            field.push_back(static_cast<char>(codepoint));
    }
}
void Application::connect() {
    if (!validIdentifier(view_.fields[2])) {
        view_.activeField = 2;
        view_.fieldSelected = !view_.fields[2].empty();
        view_.status =
            view_.fields[2].empty()
                ? "Choose a room name first. Existing names join a saved room; new names create one."
                : "Room name: 1-32 letters, numbers, _ or -.";
        return;
    }
    if (!validIdentifier(view_.fields[3], 24) || view_.fields[0].empty()) {
        view_.status = "Enter a server, room, and name. Room/name: letters, numbers, _ or -.";
        return;
    }
    try {
        std::size_t used = 0;
        int port = std::stoi(view_.fields[1], &used);
        if (used != view_.fields[1].size() || port < 1 || port > 65535)
            throw std::invalid_argument("port");
    } catch (const std::exception &) {
        view_.status = "Port must be between 1 and 65535.";
        return;
    }
    if (!view_.fields[4].empty() && (view_.fields[4].size() < 16 || !validIdentifier(view_.fields[4], 128))) {
        view_.status = "Access key: 16-128 letters, numbers, _ or -. Leave blank for open servers.";
        return;
    }
    resetBoardView();
    offlineReason_.clear();
    view_.activeField = -1;
    view_.fieldSelected = false;
    view_.connecting = true;
    view_.reconnect = true;
    view_.status = "Connecting...";
    network_.connect(view_.fields[0], view_.fields[1], view_.fields[2], view_.fields[3], view_.fields[4]);
}
void Application::action(const std::string &actionName) {
    if (actionName != "host")
        view_.stopHostArmed = false;
    if (actionName == "host") {
        toggleServer();
        return;
    }
    if (actionName != "clear" && view_.clearArmed) {
        view_.clearArmed = false;
        view_.status = "Clear canceled. Board unchanged.";
    }
    if (actionName == "theme") {
        view_.darkTheme = !view_.darkTheme;
        return;
    }
    if (actionName == "apply_color") {
        if (view_.colorText.size() != 6 ||
            view_.colorText.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
            view_.editingColor = true;
            view_.status = "Enter six hex digits, for example #7048E8, then Apply or Enter.";
            return;
        }
        view_.color = static_cast<std::uint32_t>(std::stoul(view_.colorText, nullptr, 16));
        view_.colorText = colorHex(view_.color);
        view_.editingColor = false;
        view_.colorSelected = false;
        return;
    }
    if (actionName == "clear") {
        clearBoard();
        return;
    }
    if (actionName == "connect") {
        if (view_.online || view_.connecting || view_.reconnect) {
            view_.online = false;
            view_.connecting = false;
            view_.reconnect = false;
            offlineReason_.clear();
            resetBoardView();
            view_.status = "Local board restored. Draw here or join another room.";
            network_.disconnect();
        } else
            connect();
    } else if (validDrawingTool(actionName)) {
        cancelDraft();
        view_.tool = actionName;
    } else if (actionName == "undo") {
        if (view_.clearing)
            return;
        if (view_.online)
            network_.send({{"type", "undo"}});
        else if (canDraw()) {
            if (localBoard_.undo("local").empty() && clearedLocalBackup_) {
                localBoard_ = std::move(*clearedLocalBackup_);
                clearedLocalBackup_.reset();
            }
            resetBoardView();
            view_.status = "Local undo. Export to save your board.";
        }
    } else if (actionName == "share_local") {
        shareLocalBoard();
    } else if (actionName == "export")
        exportBoard();
    else if (actionName == "import")
        importBoard();
    else if (actionName == "grid")
        view_.showGrid = !view_.showGrid;
    else if (actionName == "thinner")
        view_.strokeWidth = std::max(1.f, view_.strokeWidth - 1);
    else if (actionName == "thicker")
        view_.strokeWidth = std::min(16.f, view_.strokeWidth + 1);
    else if (actionName.starts_with("color")) {
        const auto &colors = inkColors;
        const auto index = static_cast<std::size_t>(std::stoul(actionName.substr(5)));
        if (index < colors.size()) {
            view_.color = colors[index];
            view_.colorText = colorHex(view_.color);
            view_.editingColor = false;
        }
    }
}
void Application::exportBoard() {
    try {
        if (snapshotting_ || view_.connecting || (view_.reconnect && !view_.online))
            throw std::runtime_error("Wait for the room to load, or disconnect to export your local board");
        Json shapes = Json::array();
        for (const auto &shape : view_.shapes)
            shapes.push_back(shapeJson(shape));
        writeDocumentFile(
            exportPath_, Json{{"format", "synccanvas"}, {"version", protocolVersion}, {"shapes", shapes}}, 2);
        view_.status = view_.online ? "Exported the room board to board.json."
                                    : "Exported the local board to board.json.";
    } catch (const std::exception &error) {
        view_.status = error.what();
    }
}
void Application::importBoard() {
    try {
        if (!canDraw())
            throw std::runtime_error("Wait for the room to load, or disconnect to import locally");
        if (!importQueue_.empty())
            throw std::runtime_error("An import is already running");
        auto document = readDocumentFile(exportPath_);
        Board candidate;
        candidate.load(document);
        if (!view_.online) {
            // Validate the entire additive import before replacing the local board.
            Board combined = localBoard_;
            for (const auto &shape : candidate.shapes())
                combined.add(shape, "local");
            localBoard_ = std::move(combined);
            resetBoardView();
            view_.status = "Imported into your local board. Nothing was sent to a room.";
            return;
        }
        std::size_t points = 0;
        for (const auto &shape : view_.shapes)
            points += shape.points.size();
        for (const auto &shape : candidate.shapes())
            points += shape.points.size();
        if (view_.shapes.size() + candidate.shapes().size() > maxShapes || points > 40000)
            throw std::runtime_error("Import would exceed board capacity");
        for (const auto &shape : candidate.shapes())
            importQueue_.push_back(shape);
        view_.status = "Import queued. Shapes are added to this room (not replaced).";
    } catch (const std::exception &error) {
        view_.status = error.what();
    }
}
bool Application::canDraw() const {
    return !view_.clearing && (view_.online || (!view_.connecting && !view_.reconnect));
}
bool Application::commitShape(const Shape &shape) {
    if (view_.online) {
        network_.send({{"type", "draw"}, {"shape", shapeJson(shape)}});
        return true;
    }
    if (!canDraw())
        return false;
    try {
        localBoard_.add(shape, "local");
        view_.shapes = localBoard_.shapes();
        view_.revision = localBoard_.revision();
        view_.localShapeCount = localBoard_.shapes().size();
        view_.status = "Drawing locally. Export to save; joining a room keeps this board separate.";
        return true;
    } catch (const std::exception &error) {
        view_.status = error.what();
        return false;
    }
}
void Application::shareLocalBoard() {
    try {
        if (!view_.online)
            throw std::runtime_error("Join a room before adding a copy of your local drawing");
        if (!importQueue_.empty() || view_.clearing)
            throw std::runtime_error("Wait for the current import or clear to finish");
        std::size_t points = 0;
        for (const auto &shape : view_.shapes)
            points += shape.points.size();
        for (const auto &shape : localBoard_.shapes())
            points += shape.points.size();
        if (view_.shapes.size() + localBoard_.shapes().size() > maxShapes || points > 40000)
            throw std::runtime_error("Local drawing would exceed room capacity");
        for (const auto &shape : localBoard_.shapes())
            importQueue_.push_back(shape);
        view_.status = "Adding a copy of your local drawing to this room. The local original is kept.";
    } catch (const std::exception &error) {
        view_.status = error.what();
    }
}
void Application::clearBoard() {
    if (view_.clearArmed && glfwGetTime() > clearDeadline_)
        view_.clearArmed = false;
    if (!canDraw() || !importQueue_.empty()) {
        view_.status = "Wait for the connection, import, or current clear to finish.";
        return;
    }
    const auto count = view_.online ? static_cast<std::size_t>(std::count_if(
                                          view_.shapes.begin(), view_.shapes.end(),
                                          [this](const Shape &shape) { return shape.owner == view_.selfId; }))
                                    : localBoard_.shapes().size();
    if (count == 0) {
        view_.clearArmed = false;
        view_.status = view_.online ? "No shapes from your current session to clear."
                                    : "The local board is already empty.";
        return;
    }
    if (!view_.clearArmed) {
        view_.clearArmed = true;
        clearDeadline_ = glfwGetTime() + 5;
        view_.status = view_.online
                           ? "Click Confirm clear within 5 seconds to remove only your session's shapes."
                           : "Click Confirm clear within 5 seconds. Undo can restore the local board.";
        return;
    }
    view_.clearArmed = false;
    cancelDraft();
    if (view_.online) {
        clearRemaining_ = count;
        clearAwaitingAck_ = false;
        view_.clearing = true;
        view_.status = "Clearing your shapes. Other participants' work is kept.";
    } else {
        clearedLocalBackup_ = std::move(localBoard_);
        localBoard_ = Board{};
        resetBoardView();
        view_.status = "Local board cleared. Ctrl+Z restores the previous board.";
    }
}
void Application::cancelDraft() {
    if (view_.draft && view_.online)
        network_.send({{"type", "preview"}, {"shape", nullptr}});
    view_.draft.reset();
    previewDirty_ = false;
}
void Application::resetBoardView() {
    view_.online = false;
    view_.clearArmed = false;
    view_.clearing = false;
    clearRemaining_ = 0;
    clearAwaitingAck_ = false;
    view_.shapes = localBoard_.shapes();
    view_.localShapeCount = localBoard_.shapes().size();
    view_.snapshot.clear();
    view_.peers.clear();
    view_.selfId.clear();
    view_.accessControlled = false;
    view_.revision = localBoard_.revision();
    view_.draft.reset();
    importQueue_.clear();
    previewDirty_ = false;
    snapshotting_ = false;
}
void Application::processEvents() {
    for (const auto &event : network_.poll()) {
        try {
            const auto type = event.at("type").get<std::string>();
            // Ignore queued room events after the user has explicitly left.
            if (!view_.reconnect && type != "offline")
                continue;
            if (type == "connecting")
                view_.status = "Connecting...";
            else if (type == "offline") {
                view_.online = false;
                view_.connecting = false;
                resetBoardView();
                view_.status = event.at("message").get<std::string>() +
                               (view_.reconnect ? ". Retrying in 3 seconds. Disconnect to draw locally."
                                                : ". Your local board is ready.");
                if (!offlineReason_.empty())
                    view_.status = std::exchange(offlineReason_, {});
                nextReconnect_ = glfwGetTime() + 3;
            } else if (type == "welcome") {
                if (event.at("version") != protocolVersion)
                    throw std::runtime_error("Protocol version mismatch");
                view_.selfId = event.at("id").get<std::string>();
                view_.accessControlled = event.value("access_controlled", false);
                if (event.at("room") != view_.fields[2])
                    throw std::runtime_error("Unexpected room");
                view_.peers.clear();
            } else if (type == "snapshot_begin") {
                snapshotting_ = true;
                view_.snapshot.clear();
                view_.online = false;
                view_.revision = event.at("revision").get<std::uint64_t>();
            } else if (type == "snapshot_shape" || type == "added") {
                const auto &value = event.at("shape");
                auto shape = parseShape(value);
                shape.id = value.at("id").get<std::string>();
                shape.owner = value.at("owner").get<std::string>();
                if (type == "snapshot_shape") {
                    if (!snapshotting_ || view_.snapshot.size() >= maxShapes)
                        throw std::runtime_error("Invalid snapshot");
                    view_.snapshot.push_back(std::move(shape));
                } else {
                    auto revision = event.at("revision").get<std::uint64_t>();
                    if (!view_.online || revision != view_.revision + 1 || view_.shapes.size() >= maxShapes)
                        throw std::runtime_error("Board revision mismatch");
                    view_.revision = revision;
                    if (view_.clearing && shape.owner == view_.selfId)
                        ++clearRemaining_;
                    view_.shapes.push_back(std::move(shape));
                    view_.status = "All changes synced. Revision " + std::to_string(view_.revision);
                }
            } else if (type == "snapshot_end") {
                if (!snapshotting_ || event.at("revision").get<std::uint64_t>() != view_.revision)
                    throw std::runtime_error("Incomplete snapshot");
                view_.shapes.swap(view_.snapshot);
                view_.snapshot.clear();
                snapshotting_ = false;
                view_.online = true;
                view_.connecting = false;
                view_.status = "Room " + view_.fields[2] + " loaded: " + std::to_string(view_.shapes.size()) +
                               " saved shapes. Draw together.";
            } else if (type == "removed") {
                auto revision = event.at("revision").get<std::uint64_t>();
                if (revision != view_.revision + 1)
                    throw std::runtime_error("Board revision mismatch");
                const auto id = event.at("id").get<std::string>();
                const bool clearingOwn =
                    view_.clearing &&
                    std::any_of(view_.shapes.begin(), view_.shapes.end(), [this, &id](const Shape &shape) {
                        return shape.id == id && shape.owner == view_.selfId;
                    });
                std::erase_if(view_.shapes, [&id](const Shape &shape) { return shape.id == id; });
                view_.revision = revision;
                if (clearingOwn) {
                    clearAwaitingAck_ = false;
                    if (clearRemaining_ > 0)
                        --clearRemaining_;
                    if (clearRemaining_ == 0) {
                        view_.clearing = false;
                        view_.status =
                            "Your session's shapes were cleared. Other participants' work is kept.";
                    }
                }
            } else if (type == "peer_join")
                view_.peers[event.at("id").get<std::string>()].name = event.at("name").get<std::string>();
            else if (type == "peer_leave")
                view_.peers.erase(event.at("id").get<std::string>());
            else if (type == "cursor") {
                auto &cursor = view_.peers[event.at("id").get<std::string>()];
                cursor.position = parsePoint(event.at("point"));
                cursor.visible = true;
            } else if (type == "preview") {
                const auto id = event.at("id").get<std::string>();
                const auto peer = view_.peers.find(id);
                if (view_.online && id != view_.selfId && peer != view_.peers.end()) {
                    const auto &value = event.at("shape");
                    if (value.is_null())
                        peer->second.preview.reset();
                    else
                        peer->second.preview = parseShape(value);
                }
            } else if (type == "error") {
                cancelDraft();
                importQueue_.clear();
                clearRemaining_ = 0;
                clearAwaitingAck_ = false;
                view_.clearing = false;
                view_.status = event.at("message").get<std::string>();
                if (!view_.online) {
                    offlineReason_ = view_.status;
                    view_.reconnect = false;
                    view_.connecting = false;
                    network_.disconnect();
                }
            }
        } catch (const std::exception &) {
            view_.online = false;
            view_.connecting = false;
            view_.status = "Invalid server state; reconnecting.";
            network_.disconnect();
            nextReconnect_ = glfwGetTime() + 3;
        }
    }
}
void Application::input() {
    if (view_.stopHostArmed && glfwGetTime() > stopHostDeadline_)
        view_.stopHostArmed = false;
    if (view_.hosting && !hostedServer_.running() && view_.hostStatus != "Host failed / click Stop server") {
        view_.hostStatus = "Host failed / click Stop server";
        view_.status = hostedServer_.error();
    }
    const double now = glfwGetTime();
    if (view_.clearArmed && now > clearDeadline_) {
        view_.clearArmed = false;
        view_.status = "Clear canceled. Board unchanged.";
    }
    if (view_.online && clearRemaining_ > 0 && !clearAwaitingAck_ && now - lastClear_ >= .025) {
        network_.send({{"type", "undo"}});
        clearAwaitingAck_ = true;
        lastClear_ = now;
    }
    if (view_.reconnect && !view_.online && !view_.connecting && now >= nextReconnect_)
        connect();
    if (view_.online && !importQueue_.empty() && now - lastImport_ > .025) {
        network_.send({{"type", "draw"}, {"shape", shapeJson(importQueue_.front())}});
        importQueue_.pop_front();
        lastImport_ = now;
    }
    if (pressed(GLFW_KEY_ESCAPE)) {
        if (view_.stopHostArmed) {
            view_.stopHostArmed = false;
            view_.status = "Server stop canceled. Hosting continues.";
        } else if (view_.draft)
            cancelDraft();
        else if (view_.clearArmed) {
            view_.clearArmed = false;
            view_.status = "Clear canceled. Board unchanged.";
        } else if (view_.editingColor)
            view_.editingColor = false;
        else if (view_.activeField >= 0)
            view_.activeField = -1;
        else
            glfwSetWindowShouldClose(window_, true);
    }
    if (pressed(GLFW_KEY_TAB) && !view_.editingColor && !view_.online && !view_.connecting &&
        !view_.reconnect) {
        do {
            view_.activeField = (view_.activeField + 1) % 5;
        } while (!fieldEditable(view_.activeField));
        view_.fieldSelected = true;
    }
    if (pressed(GLFW_KEY_BACKSPACE)) {
        if (view_.editingColor) {
            if (view_.colorSelected)
                view_.colorText.clear();
            else if (!view_.colorText.empty())
                view_.colorText.pop_back();
            view_.colorSelected = false;
        } else if (view_.activeField >= 0 && fieldEditable(view_.activeField)) {
            auto &field = view_.fields[view_.activeField];
            if (view_.fieldSelected)
                field.clear();
            else if (!field.empty())
                field.pop_back();
            view_.fieldSelected = false;
        }
    }
    if (pressed(GLFW_KEY_ENTER)) {
        if (view_.editingColor)
            action("apply_color");
        else if (!view_.online && !view_.connecting && !view_.reconnect)
            connect();
    }
    if (exportRequested_)
        exportBoard();
    if (importRequested_)
        importBoard();
    exportRequested_ = false;
    importRequested_ = false;
    if (requestedTool_ >= 0 && view_.activeField < 0 && !view_.editingColor && !view_.draft)
        action(std::string(drawingTools[static_cast<std::size_t>(requestedTool_)].kind));
    requestedTool_ = -1;
    if (view_.activeField < 0 && !view_.editingColor) {
        if (pressed(GLFW_KEY_LEFT_BRACKET))
            action("thinner");
        if (pressed(GLFW_KEY_RIGHT_BRACKET))
            action("thicker");
        if (undoRequested_) {
            action("undo");
            undoRequested_ = false;
        }
    }
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    int ww, hh, fw, fh;
    glfwGetWindowSize(window_, &ww, &hh);
    glfwGetFramebufferSize(window_, &fw, &fh);
    if (ww <= 0 || hh <= 0)
        return;
    float x = float(mx * fw / ww), y = float(my * fh / hh);
    view_.hoveredTool.clear();
    view_.hoveredButton.clear();
    for (const auto &button : layout_.buttons(view_))
        if (button.enabled && button.bounds.contains(x, y)) {
            view_.hoveredButton = button.action;
            if (validDrawingTool(button.action))
                view_.hoveredTool = button.action;
        }
    if (view_.online && view_.draft && previewDirty_ && now - lastPreview_ >= .05) {
        network_.send({{"type", "preview"}, {"shape", shapeJson(*view_.draft)}});
        lastPreview_ = now;
        previewDirty_ = false;
    }
    if (view_.online && layout_.onBoard(x, y) && now - lastCursor_ > .05) {
        auto point = layout_.toBoard(x, y);
        network_.send({{"type", "cursor"}, {"point", {point.x, point.y}}});
        lastCursor_ = now;
    }
}
void Application::pointer(double mx, double my, int phase) {
    int ww, hh, fw, fh;
    glfwGetWindowSize(window_, &ww, &hh);
    glfwGetFramebufferSize(window_, &fw, &fh);
    if (ww <= 0 || hh <= 0 || fw <= 0 || fh <= 0)
        return;
    layout_.resize(fw, fh);
    float x = float(mx * fw / ww), y = float(my * fh / hh);
    if (phase == 2 && !view_.pressedButton.empty()) {
        const auto pressedAction = std::exchange(view_.pressedButton, {});
        for (const auto &button : layout_.buttons(view_))
            if (button.enabled && button.action == pressedAction && button.bounds.contains(x, y)) {
                action(button.action);
                break;
            }
        return;
    }
    if (phase == 1) {
        view_.pressedButton.clear();
        if (view_.stopHostArmed) {
            const auto buttons = layout_.buttons(view_);
            if (std::none_of(buttons.begin(), buttons.end(), [x, y](const Button &button) {
                    return button.action == "host" && button.bounds.contains(x, y);
                }))
                view_.stopHostArmed = false;
        }
        if (view_.clearArmed) {
            const auto buttons = layout_.buttons(view_);
            if (std::none_of(buttons.begin(), buttons.end(), [x, y](const Button &button) {
                    return button.action == "clear" && button.bounds.contains(x, y);
                }))
                view_.clearArmed = false;
        }
        bool handled = false;
        if (layout_.colorField().contains(x, y)) {
            view_.editingColor = true;
            view_.colorSelected = true;
            view_.activeField = -1;
            handled = true;
        }
        for (int i = 0; i < 5; ++i)
            if (layout_.field(i).contains(x, y)) {
                if (fieldEditable(i)) {
                    view_.activeField = i;
                    view_.editingColor = false;
                    view_.fieldSelected = false;
                    if (i == 2)
                        view_.status =
                            "Choose a room: an existing name joins its board; a new name creates one.";
                } else
                    view_.status = "Disconnect or stop hosting before changing this field.";
                handled = true;
            }
        if (!handled) {
            view_.activeField = -1;
            view_.editingColor = false;
        }
        for (const auto &button : layout_.buttons(view_))
            if (button.bounds.contains(x, y)) {
                if (button.enabled)
                    view_.pressedButton = button.action;
                handled = true;
                break;
            }
        if (!handled && canDraw() && layout_.onBoard(x, y) && importQueue_.empty()) {
            auto point = layout_.toBoard(x, y);
            Shape shape;
            shape.kind = view_.tool;
            shape.color = view_.color;
            shape.width = view_.strokeWidth;
            shape.points = {point, point};
            view_.draft = std::move(shape);
            previewDirty_ = true;
        }
    }
    if (phase != 1 && view_.draft) {
        previewDirty_ = true;
        auto point = layout_.toBoard(x, y);
        auto &draft = *view_.draft;
        const bool constrain = glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                               glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        if (constrain && draft.kind != "pen")
            point = constrainEndpoint(draft.points.front(), point, draft.kind);
        if (draft.kind == "pen") {
            auto previous = draft.points.back();
            if (std::hypot(previous.x - point.x, previous.y - point.y) > 2) {
                if (draft.points.size() < maxPoints)
                    draft.points.push_back(point);
                else {
                    if (!commitShape(draft)) {
                        cancelDraft();
                        return;
                    }
                    draft.points = {previous, point};
                }
            }
        } else
            draft.points.back() = point;
    }
    if (phase == 2 && view_.draft) {
        commitShape(*view_.draft);
        view_.draft.reset();
        previewDirty_ = false;
    }
}

int Application::run() {
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    window_ = glfwCreateWindow(1440, 940, "SyncCanvas | collaborative workspace", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return 2;
    }
    glfwSetWindowSizeLimits(window_, 1200, 900, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress)) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        return 3;
    }
    glfwSetWindowUserPointer(window_, this);
    glfwGetCursorPos(window_, &pointerX_, &pointerY_);
    glfwSetMouseButtonCallback(window_, [](GLFWwindow *window, int button, int state, int) {
        if (button != GLFW_MOUSE_BUTTON_LEFT)
            return;
        auto *self = static_cast<Application *>(glfwGetWindowUserPointer(window));
        self->pointer(self->pointerX_, self->pointerY_, state == GLFW_PRESS ? 1 : 2);
    });
    glfwSetCursorPosCallback(window_, [](GLFWwindow *window, double x, double y) {
        auto *self = static_cast<Application *>(glfwGetWindowUserPointer(window));
        self->pointerX_ = x;
        self->pointerY_ = y;
        if (self->view_.draft)
            self->pointer(x, y, 0);
    });
    glfwSetWindowFocusCallback(window_, [](GLFWwindow *window, int focused) {
        if (!focused) {
            auto *self = static_cast<Application *>(glfwGetWindowUserPointer(window));
            self->cancelDraft();
            self->view_.pressedButton.clear();
        }
    });
    glfwSetKeyCallback(window_, [](GLFWwindow *window, int key, int, int state, int mods) {
        auto *self = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (state == GLFW_PRESS && key != GLFW_KEY_ESCAPE)
            self->view_.stopHostArmed = false;
        if (state == GLFW_PRESS && key != GLFW_KEY_ESCAPE)
            self->view_.clearArmed = false;
        if (key >= 0 && key < 350 &&
            (state == GLFW_PRESS || (key == GLFW_KEY_BACKSPACE && state == GLFW_REPEAT)))
            self->keyEvents_[key] = true;
        if (state == GLFW_PRESS && !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER)) &&
            self->view_.activeField < 0 && !self->view_.editingColor && !self->view_.draft)
            for (std::size_t i = 0; i < drawingTools.size(); ++i)
                if (key == drawingTools[i].shortcut || key == GLFW_KEY_1 + static_cast<int>(i))
                    self->requestedTool_ = static_cast<int>(i);
        if (self->view_.editingColor && state == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
            if (key == GLFW_KEY_A)
                self->view_.colorSelected = true;
            if (key == GLFW_KEY_V) {
                if (const char *clipboard = glfwGetClipboardString(window)) {
                    std::string value;
                    for (std::size_t i = 0; i < 8 && clipboard[i]; ++i)
                        value.push_back(clipboard[i]);
                    if (value.starts_with('#'))
                        value.erase(0, 1);
                    const auto candidate =
                        (self->view_.colorSelected ? std::string{} : self->view_.colorText) + value;
                    if (candidate.size() <= 6 &&
                        candidate.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                        self->view_.colorText = candidate;
                        self->view_.colorSelected = false;
                    } else
                        self->view_.status = "Paste a hex color such as #7048E8.";
                }
            }
        }
        const bool editable = self->view_.activeField >= 0 && self->fieldEditable(self->view_.activeField);
        if (editable && state == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
            if (key == GLFW_KEY_A)
                self->view_.fieldSelected = true;
            if (key == GLFW_KEY_V) {
                if (const char *clipboard = glfwGetClipboardString(window)) {
                    std::string value;
                    for (std::size_t i = 0; i <= 128 && clipboard[i]; ++i)
                        value.push_back(clipboard[i]);
                    const int field = self->view_.activeField;
                    const auto &current = self->view_.fields[field];
                    const auto size = (self->view_.fieldSelected ? 0 : current.size()) + value.size();
                    if (size > fieldLimit(field) ||
                        !std::all_of(value.begin(), value.end(),
                                     [field](unsigned char c) { return allowedFieldCharacter(field, c); }))
                        self->view_.status = "Paste rejected: invalid characters or field length.";
                    else if (!value.empty()) {
                        if (self->view_.fieldSelected)
                            self->view_.fields[field].clear();
                        self->view_.fields[field] += value;
                        self->view_.fieldSelected = false;
                    }
                }
            }
        }
        if (key == GLFW_KEY_G && state == GLFW_PRESS &&
            !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER)) && self->view_.activeField < 0 &&
            !self->view_.editingColor)
            self->action("grid");
        if (key == GLFW_KEY_S && state == GLFW_PRESS && (mods & GLFW_MOD_CONTROL))
            self->exportRequested_ = true;
        if (key == GLFW_KEY_O && state == GLFW_PRESS && (mods & GLFW_MOD_CONTROL))
            self->importRequested_ = true;
        if (key == GLFW_KEY_Z && state == GLFW_PRESS && (mods & GLFW_MOD_CONTROL) &&
            self->view_.activeField < 0 && !self->view_.editingColor)
            self->undoRequested_ = true;
    });
    glfwSetCharCallback(window_, [](GLFWwindow *window, unsigned codepoint) {
        static_cast<Application *>(glfwGetWindowUserPointer(window))->character(codepoint);
    });
    if (!renderer_.init()) {
        std::cerr << "No supported system font found; install DejaVu Sans or Segoe UI.\n";
        renderer_.destroy();
        glfwDestroyWindow(window_);
        glfwTerminate();
        return 4;
    }
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        processEvents();
        int w, h;
        glfwGetFramebufferSize(window_, &w, &h);
        if (w <= 0 || h <= 0) {
            glfwWaitEventsTimeout(.05);
            continue;
        }
        layout_.resize(w, h);
        input();
        glViewport(0, 0, w, h);
        glClearColor(.969f, .969f, .984f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer_.draw(view_, layout_);
        glfwSwapBuffers(window_);
    }
    network_.disconnect();
    if (!hostedServer_.stop())
        std::cerr << "Hosted server stopped with unsaved room changes.\n";
    renderer_.destroy();
    glfwDestroyWindow(window_);
    glfwTerminate();
    return 0;
}
} // namespace synccanvas
