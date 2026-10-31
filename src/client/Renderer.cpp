#include "synccanvas/ClientView.hpp"
#include <algorithm>
#include <cmath>
namespace synccanvas {
using vectorgl::Color;
void ClientLayout::resize(int w, int h) {
    width = w;
    height = h;
    scale = std::max(.1f, std::min((w - 320.f) / boardWidth, (h - 240.f) / boardHeight));
    left = 280 + (w - 280 - boardWidth * scale) * .5f;
    top = 180 + (h - 230 - boardHeight * scale) * .5f;
}
Point ClientLayout::toBoard(float x, float y) const {
    return {std::clamp((x - left) / scale, 0.f, boardWidth), std::clamp((y - top) / scale, 0.f, boardHeight)};
}
bool ClientLayout::onBoard(float x, float y) const {
    return Rect{left, top, boardWidth * scale, boardHeight * scale}.contains(x, y);
}
Rect ClientLayout::field(int index) const {
    if (index == 0)
        return {22, 127, 150, 34};
    if (index == 1)
        return {182, 127, 68, 34};
    return {22, 190.f + (index - 2) * 63, 228, 34};
}
Rect ClientLayout::toolbar() const {
    return {280 + (width - 280 - 340.f) * .5f, 100, 340, 64};
}
std::vector<Button> ClientLayout::buttons(const ClientView &view) const {
    std::vector<Button> result{
        {{22, 380, 228, 38},
         "connect",
         view.online || view.connecting || view.reconnect ? "Disconnect" : "Join room"},
        {{22, 592, 228, 36}, "undo", "Undo my last shape"},
        {{22, 642, 228, 36}, "export", "Export board.json"},
        {{22, 687, 228, 36}, "import", "Import board.json"},
        {{22, 749, 48, 32}, "thinner", "-"},
        {{202, 749, 48, 32}, "thicker", "+"}};
    result.push_back({{float(width - 166), 110, 140, 36}, "grid", "Grid  G", view.showGrid});
    const auto bar = toolbar();
    for (std::size_t i = 0; i < drawingTools.size(); ++i) {
        const auto &tool = drawingTools[i];
        result.push_back({{bar.x + 8 + static_cast<float>(i) * 54, bar.y + 8, 48, 48},
                          std::string(tool.kind),
                          std::string(tool.label),
                          view.tool == tool.kind});
    }
    const auto &colors = inkColors;
    for (int i = 0; i < 5; ++i)
        result.push_back(
            {{22.f + i * 47, 538, 40, 32}, "color" + std::to_string(i), "", view.color == colors[i]});
    return result;
}
bool Renderer::init() {
    canvas_.init();
    for (const char *font :
         {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
          "/System/Library/Fonts/Supplemental/Arial.ttf"}) {
        if (canvas_.setFont(font, 16)) {
            hasFont_ = true;
            break;
        }
    }
    return hasFont_;
}
void Renderer::destroy() {
    canvas_.destroy();
}
void Renderer::text(const std::string &value, float x, float y, std::uint32_t color) {
    if (!hasFont_)
        return;
    canvas_.setFillColor(Color::hex(color));
    canvas_.fillText(value, x, y - 16);
}
void Renderer::shape(const Shape &value) {
    if (value.points.size() < 2)
        return;
    canvas_.setStrokeColor(Color::hex(value.color));
    canvas_.setLineWidth(value.width);
    const auto a = value.points.front(), b = value.points.back();
    if (value.kind == "pen") {
        canvas_.beginPath();
        canvas_.moveTo(a.x, a.y);
        for (std::size_t i = 1; i < value.points.size(); ++i)
            canvas_.lineTo(value.points[i].x, value.points[i].y);
        canvas_.stroke();
        canvas_.setFillColor(Color::hex(value.color));
        canvas_.fillCircle(a.x, a.y, value.width * .5f);
        canvas_.fillCircle(b.x, b.y, value.width * .5f);
    } else if (value.kind == "rect") {
        canvas_.strokeRect(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y));
    } else if (value.kind == "diamond") {
        const float cx = (a.x + b.x) * .5f, cy = (a.y + b.y) * .5f;
        canvas_.beginPath();
        canvas_.moveTo(cx, std::min(a.y, b.y));
        canvas_.lineTo(std::max(a.x, b.x), cy);
        canvas_.lineTo(cx, std::max(a.y, b.y));
        canvas_.lineTo(std::min(a.x, b.x), cy);
        canvas_.closePath();
        canvas_.stroke();
    } else if (value.kind == "line" || value.kind == "arrow") {
        canvas_.beginPath();
        canvas_.moveTo(a.x, a.y);
        canvas_.lineTo(b.x, b.y);
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        if (value.kind == "arrow" && length > .01f) {
            const float ux = (b.x - a.x) / length, uy = (b.y - a.y) / length;
            const float head = std::min(length * .45f, std::max(12.f, value.width * 4));
            canvas_.moveTo(b.x - ux * head - uy * head * .5f, b.y - uy * head + ux * head * .5f);
            canvas_.lineTo(b.x, b.y);
            canvas_.lineTo(b.x - ux * head + uy * head * .5f, b.y - uy * head - ux * head * .5f);
        }
        canvas_.stroke();
    } else if (value.kind == "ellipse") {
        canvas_.strokeEllipse((a.x + b.x) * .5f, (a.y + b.y) * .5f, std::max(.1f, std::abs(a.x - b.x) * .5f),
                              std::max(.1f, std::abs(a.y - b.y) * .5f));
    }
}
void Renderer::draw(const ClientView &view, const ClientLayout &layout) {
    canvas_.beginFrame(layout.width, layout.height);
    canvas_.setFillColor(Color::hex(0xffffff));
    canvas_.fillRoundedRect(10, 10, 252, float(layout.height - 20), 14);
    canvas_.setStrokeColor(Color::hex(0xe9ecef));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(10, 10, 252, float(layout.height - 20), 14);
    text("S Y N C C A N V A S", 22, 42, 0x6965db);
    text("Ideas, on the same page.", 22, 68, 0x868e96);
    const char *labels[]{"SERVER", "PORT", "ROOM", "YOUR NAME", "ACCESS KEY"};
    const bool fieldsLocked = view.online || view.connecting || view.reconnect;
    for (int i = 0; i < 5; ++i) {
        auto box = layout.field(i);
        text(labels[i], box.x, box.y - 9, 0x868e96);
        canvas_.setFillColor(Color::hex(view.activeField == i ? 0xf3f0ff : 0xf1f3f5));
        canvas_.fillRoundedRect(box.x, box.y, box.w, box.h, 5);
        auto value = view.fields[i];
        if (i == 4)
            value = value.empty() ? "Optional" : std::string(std::min<std::size_t>(value.size(), 20), '*');
        const std::size_t visible = i == 0 ? 16 : i == 1 ? 5 : 24;
        if (value.size() > visible)
            value = "..." + value.substr(value.size() - (visible - 3));
        if (!fieldsLocked && view.activeField == i && view.fieldSelected) {
            canvas_.setFillColor(Color::hex(0xd0bfff));
            canvas_.fillRoundedRect(box.x + 7, box.y + 5, box.w - 14, box.h - 10, 3);
        }
        text(value + (!fieldsLocked && view.activeField == i ? "|" : ""), box.x + 10, box.y + 23,
             fieldsLocked ? 0x868e96 : 0x343a40);
    }
    text("DRAWING TOOL", 22, 447, 0x868e96);
    for (const auto &tool : drawingTools)
        if (tool.kind == (view.hoveredTool.empty() ? view.tool : view.hoveredTool))
            text(std::string(tool.label) + "  (" + tool.shortcut + ")", 22, 479, 0x343a40);
    text("Choose above, or press 1-6", 22, 504, 0x868e96);
    const auto bar = layout.toolbar();
    canvas_.setFillColor(Color::hex(0xe9e8f0));
    canvas_.fillRoundedRect(bar.x - 2, bar.y + 3, bar.w + 4, bar.h, 12);
    canvas_.setFillColor(Color::hex(0xffffff));
    canvas_.fillRoundedRect(bar.x, bar.y, bar.w, bar.h, 12);
    canvas_.setStrokeColor(Color::hex(0xe9ecef));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(bar.x, bar.y, bar.w, bar.h, 12);
    text("STROKE", 22, 526, 0x868e96);
    const auto &colors = inkColors;
    for (const auto &button : layout.buttons(view)) {
        const auto &b = button.bounds;
        const bool toolButton = validDrawingTool(button.action);
        bool swatch = button.action.starts_with("color");
        auto color = swatch ? colors[button.action.back() - '0'] : button.selected ? 0xe5dbff : 0xf1f3f5;
        if (toolButton)
            color = button.selected ? 0xe5dbff : view.hoveredTool == button.action ? 0xe9ecef : 0xffffff;
        if (button.action == "connect")
            color = view.online ? 0xd3f9d8 : 0xe5dbff;
        canvas_.setFillColor(Color::hex(color));
        canvas_.fillRoundedRect(b.x, b.y, b.w, b.h, 5);
        if (swatch && button.selected) {
            canvas_.setStrokeColor(Color::hex(0x6965db));
            canvas_.setLineWidth(2);
            canvas_.strokeRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
        }
        if (toolButton) {
            Shape icon;
            icon.kind = button.action;
            icon.color = button.selected ? 0x5f3dc4 : 0x495057;
            icon.width = 1.8f;
            icon.points = {{b.x + 13, b.y + 11}, {b.x + 33, b.y + 31}};
            if (icon.kind == "arrow" || icon.kind == "line")
                icon.points = {{b.x + 12, b.y + 30}, {b.x + 33, b.y + 12}};
            if (icon.kind == "pen")
                icon.points = {{b.x + 11, b.y + 28},
                               {b.x + 16, b.y + 15},
                               {b.x + 21, b.y + 27},
                               {b.x + 27, b.y + 13},
                               {b.x + 34, b.y + 22}};
            shape(icon);
            for (std::size_t i = 0; i < drawingTools.size(); ++i)
                if (drawingTools[i].kind == button.action)
                    text(std::to_string(i + 1), b.x + 36, b.y + 45, 0x868e96);
        } else
            text(button.label, b.x + 10, b.y + 23, 0x343a40);
    }
    text(std::to_string(int(view.strokeWidth)) + " px", 111, 771, 0x343a40);
    text("Ctrl+Z  undo   [ ]  width", 22, 807, 0x868e96);
    text("Ctrl+S  export   Ctrl+O  import", 22, 830, 0x868e96);
    text(view.online ? (view.accessControlled ? "Key required / plain TCP" : "Open server / plain TCP")
                     : "Local / trusted LAN",
         22, float(layout.height - 46), 0x868e96);
    text("Draw. Share. Think together.", 22, float(layout.height - 23), 0x868e96);
    text(view.online ? "LIVE WORKSPACE" : "A LITTLE SPACE FOR BIG IDEAS", 300, 41, 0x6965db);
    text("Room / " + view.fields[2], 300, 72, 0x343a40);
    text(std::to_string(view.peers.size()) + " collaborators    |    " + std::to_string(view.shapes.size()) +
             " shapes",
         float(layout.width - 370), 72, 0x868e96);
    canvas_.setStrokeColor(Color::hex(0xe2e2eb));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(layout.left - 1, layout.top - 1, boardWidth * layout.scale + 2,
                              boardHeight * layout.scale + 2, 2);
    canvas_.save();
    canvas_.clipRect(layout.left, layout.top, boardWidth * layout.scale, boardHeight * layout.scale);
    canvas_.translate(layout.left, layout.top);
    canvas_.scale(layout.scale, layout.scale);
    canvas_.setFillColor(Color::hex(0xffffff));
    canvas_.fillRect(0, 0, boardWidth, boardHeight);
    canvas_.setFillColor(Color::hex(0xe9ecef));
    if (view.showGrid)
        for (int x = 20; x < int(boardWidth); x += 24)
            for (int y = 20; y < int(boardHeight); y += 24)
                canvas_.fillCircle(float(x), float(y), .8f);
    for (const auto &value : view.shapes)
        shape(value);
    for (const auto &[id, cursor] : view.peers)
        if (id != view.selfId && cursor.preview)
            shape(*cursor.preview);
    if (view.draft)
        shape(*view.draft);
    for (const auto &[id, cursor] : view.peers) {
        if (id == view.selfId || !cursor.visible)
            continue;
        canvas_.setStrokeColor(Color::hex(0x9c36b5));
        canvas_.setLineWidth(2);
        canvas_.strokeCircle(cursor.position.x, cursor.position.y, 7);
        text(cursor.name, cursor.position.x + 12, cursor.position.y - 5, 0x9c36b5);
    }
    const bool remoteDrawing = std::any_of(view.peers.begin(), view.peers.end(),
                                           [](const auto &peer) { return peer.second.preview.has_value(); });
    if (view.shapes.empty() && !view.draft && !remoteDrawing) {
        text(view.online ? "Choose a tool and draw. Invite someone to the same room."
                         : "Join a room to start drawing together.",
             300, 375, 0x868e96);
    }
    canvas_.restore();
    text(view.status.substr(0, 110), 300, float(layout.height - 28), view.online ? 0x2b8a3e : 0x9c36b5);
    canvas_.endFrame();
}
} // namespace synccanvas
