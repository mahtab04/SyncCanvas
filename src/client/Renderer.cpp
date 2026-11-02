#include "synccanvas/ClientView.hpp"
#include <algorithm>
#include <cmath>
#include <vectorgl/ui.hpp>
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
        return {22, 111, 150, 34};
    if (index == 1)
        return {182, 111, 68, 34};
    return {22, 250.f + (index - 2) * 59, 228, 34};
}
Rect ClientLayout::toolbar() const {
    return {280 + (width - 280 - 340.f) * .5f, 100, 340, 64};
}
Rect ClientLayout::colorField() const {
    return {22, 662, 150, 34};
}
std::vector<Button> ClientLayout::buttons(const ClientView &view) const {
    std::vector<Button> result{
        {{22, 155, 228, 38},
         "host",
         view.hosting ? (view.stopHostArmed ? "Confirm stop" : "Stop server") : "Start server"},
        {{22, 428, 228, 38},
         "connect",
         view.online || view.connecting || view.reconnect ? "Disconnect" : "Join / create room"},
        {{22, 760, 108, 36}, "undo", "Undo"},
        {{140, 760, 110, 36},
         "clear",
         view.clearing     ? "Clearing..."
         : view.clearArmed ? "Confirm clear"
         : view.online     ? "Clear mine"
                           : "Clear board"},
        {{22, 806, 108, 36}, "export", "Export"},
        {{140, 806, 110, 36}, "import", "Import"},
        {{22, 714, 40, 30}, "thinner", "-"},
        {{210, 714, 40, 30}, "thicker", "+"},
        {{182, 662, 68, 34}, "apply_color", "Apply"},
        {{float(width - 164), 20, 138, 34}, "theme", view.darkTheme ? "Light theme" : "Dark theme"}};
    result.push_back({{float(width - 166), 110, 140, 36}, "grid", "Grid  G", view.showGrid});
    if (view.online && view.localShapeCount > 0)
        result.push_back(
            {{300, 110, 180, 36}, "share_local", "Add local (" + std::to_string(view.localShapeCount) + ")"});
    const auto bar = toolbar();
    for (std::size_t i = 0; i < drawingTools.size(); ++i) {
        const auto &tool = drawingTools[i];
        result.push_back({{bar.x + 8 + static_cast<float>(i) * 54, bar.y + 8, 48, 48},
                          std::string(tool.kind),
                          std::string(tool.label),
                          view.tool == tool.kind});
    }
    const auto &colors = inkColors;
    for (std::size_t i = 0; i < colors.size(); ++i)
        result.push_back(
            {{22.f + static_cast<float>(i % 6) * 38, 586.f + static_cast<float>(i / 6) * 36, 32, 28},
             "color" + std::to_string(i),
             "",
             view.color == colors[i]});
    return result;
}
bool Renderer::init() {
    canvas_.init();
    for (const char *font :
         {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
          "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
          "/System/Library/Fonts/Supplemental/Arial.ttf"}) {
        if (canvas_.setFont(font, 16)) {
            hasFont_ = true;
            fontPath_ = font;
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
    if (darkTheme_ && drawingBoard_ && ((value.color >> 16) & 255) < 64 && ((value.color >> 8) & 255) < 64 &&
        (value.color & 255) < 64) {
        strokeShape(value, 0xc5cfdf, value.width + 2);
    }
    strokeShape(value, value.color, value.width);
}
void Renderer::strokeShape(const Shape &value, std::uint32_t color, float width) {
    if (value.points.size() < 2)
        return;
    canvas_.setStrokeColor(Color::hex(color));
    canvas_.setLineWidth(width);
    const auto a = value.points.front(), b = value.points.back();
    if (value.kind == "pen") {
        canvas_.beginPath();
        canvas_.moveTo(a.x, a.y);
        for (std::size_t i = 1; i < value.points.size(); ++i)
            canvas_.lineTo(value.points[i].x, value.points[i].y);
        canvas_.stroke();
        canvas_.setFillColor(Color::hex(color));
        canvas_.fillCircle(a.x, a.y, width * .5f);
        canvas_.fillCircle(b.x, b.y, width * .5f);
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
    darkTheme_ = view.darkTheme;
    drawingBoard_ = false;
    const auto panel = view.darkTheme ? 0x171e2cU : 0xffffffU;
    const auto textColor = view.darkTheme ? 0xe7edf7U : 0x343a40U;
    const auto muted = view.darkTheme ? 0x9baac1U : 0x868e96U;
    const auto fieldColor = view.darkTheme ? 0x232e42U : 0xf1f3f5U;
    const auto border = view.darkTheme ? 0x35435cU : 0xe9ecefU;
    const auto accent = view.darkTheme ? 0xb6a6ffU : 0x6965dbU;
    const auto selection = view.darkTheme ? 0x493b72U : 0xe5dbffU;
    canvas_.beginFrame(layout.width, layout.height);
    canvas_.setFillColor(Color::hex(view.darkTheme ? 0x101622 : 0xf7f7fb));
    canvas_.fillRect(0, 0, float(layout.width), float(layout.height));
    canvas_.setFillColor(Color::hex(panel));
    canvas_.fillRoundedRect(10, 10, 252, float(layout.height - 20), 14);
    canvas_.setStrokeColor(Color::hex(border));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(10, 10, 252, float(layout.height - 20), 14);
    text("S Y N C C A N V A S", 22, 42, accent);
    text("Ideas, on the same page.", 22, 68, muted);
    const char *labels[]{"SERVER", "PORT", "ROOM NAME (required)", "YOUR NAME", "ACCESS KEY"};
    for (int i = 0; i < 5; ++i) {
        const bool fieldsLocked = view.online || view.connecting || view.reconnect ||
                                  (view.hosting && (i == 0 || i == 1 || i == 4));
        auto box = layout.field(i);
        text(labels[i], box.x, box.y - 9, muted);
        canvas_.setFillColor(Color::hex(view.activeField == i ? selection : fieldColor));
        canvas_.fillRoundedRect(box.x, box.y, box.w, box.h, 8);
        canvas_.setStrokeColor(Color::hex(view.activeField == i && !fieldsLocked ? accent : border));
        canvas_.setLineWidth(view.activeField == i && !fieldsLocked ? 1.5f : 1.f);
        canvas_.strokeRoundedRect(box.x, box.y, box.w, box.h, 8);
        auto value = view.fields[i];
        const bool placeholder = value.empty() && view.activeField != i && (i == 2 || i == 4);
        if (i == 4 && !value.empty())
            value = std::string(std::min<std::size_t>(value.size(), 20), '*');
        if (placeholder)
            value = i == 2 ? "Choose a room name" : "Optional";
        const std::size_t visible = i == 0 ? 16 : i == 1 ? 5 : 24;
        if (value.size() > visible)
            value = "..." + value.substr(value.size() - (visible - 3));
        if (!fieldsLocked && view.activeField == i && view.fieldSelected) {
            canvas_.setFillColor(Color::hex(selection));
            canvas_.fillRoundedRect(box.x + 7, box.y + 5, box.w - 14, box.h - 10, 3);
        }
        text(value + (!fieldsLocked && view.activeField == i ? "|" : ""), box.x + 10, box.y + 23,
             fieldsLocked || placeholder ? muted : textColor);
    }
    canvas_.setFillColor(Color::hex(view.hosting ? 0x2f9e44U : muted));
    canvas_.fillCircle(305, 89, 3);
    text(view.hostStatus, 316, 95, view.hosting ? accent : muted);
    canvas_.setStrokeColor(Color::hex(border));
    canvas_.setLineWidth(1);
    canvas_.beginPath();
    canvas_.moveTo(22, 480);
    canvas_.lineTo(250, 480);
    canvas_.stroke();
    text("DRAWING TOOL", 22, 495, muted);
    for (const auto &tool : drawingTools)
        if (tool.kind == (view.hoveredTool.empty() ? view.tool : view.hoveredTool))
            text(std::string(tool.label) + "  (" + tool.shortcut + ")", 22, 527, textColor);
    text("Choose above, or press 1-6", 22, 552, muted);
    const auto bar = layout.toolbar();
    canvas_.setFillColor(Color::hex(view.darkTheme ? 0x080d17 : 0xe9e8f0));
    canvas_.fillRoundedRect(bar.x - 2, bar.y + 3, bar.w + 4, bar.h, 12);
    canvas_.setFillColor(Color::hex(panel));
    canvas_.fillRoundedRect(bar.x, bar.y, bar.w, bar.h, 12);
    canvas_.setStrokeColor(Color::hex(border));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(bar.x, bar.y, bar.w, bar.h, 12);
    text("STROKE COLOR", 22, 574, muted);
    const auto &colors = inkColors;
    for (const auto &button : layout.buttons(view)) {
        const auto &b = button.bounds;
        const bool toolButton = validDrawingTool(button.action);
        const bool hovered = button.enabled && view.hoveredButton == button.action;
        const bool pressed = button.enabled && view.pressedButton == button.action;
        bool swatch = button.action.starts_with("color");
        auto color = swatch            ? colors[static_cast<std::size_t>(std::stoul(button.action.substr(5)))]
                     : button.selected ? selection
                                       : fieldColor;
        if (!toolButton && !swatch) {
            const bool primary =
                button.action == "connect" && !view.online && !view.connecting && !view.reconnect;
            const bool danger = (button.action == "host" && view.hosting) ||
                                (button.action == "connect" && !primary) ||
                                (button.action == "clear" && view.clearArmed);
            const auto fill = primary           ? (view.darkTheme ? 0x8c76edU : 0x6965dbU)
                              : danger          ? (view.darkTheme ? 0x3e2834U : 0xfff1f2U)
                              : button.selected ? selection
                                                : panel;
            const auto hoverFill = primary  ? (view.darkTheme ? 0x9e8af5U : 0x5954cbU)
                                   : danger ? (view.darkTheme ? 0x55303fU : 0xffe1e5U)
                                            : (view.darkTheme ? 0x2c3750U : 0xf1f0ffU);
            const auto pressFill = primary  ? (view.darkTheme ? 0x7861d7U : 0x4c47b5U)
                                   : danger ? (view.darkTheme ? 0x68394aU : 0xffcdd5U)
                                            : selection;
            const auto outline = primary ? fill : danger ? (view.darkTheme ? 0x704354U : 0xf5bec9U) : border;
            const auto labelInk = primary  ? (view.darkTheme ? 0x171325U : 0xffffffU)
                                  : danger ? (view.darkTheme ? 0xffa7b7U : 0xb42343U)
                                           : textColor;
            if (button.action == "connect" || button.action == "host") {
                canvas_.setFillColor(Color::hex(view.darkTheme ? 0x0e1420 : 0xe9e8f4));
                canvas_.fillRoundedRect(b.x, b.y + (pressed ? 1 : 3), b.w, b.h, 10);
            }
            vectorgl::ui::Button widget;
            widget.setBounds(b.x, b.y, b.w, b.h);
            widget.setText(button.label);
            widget.setFont(fontPath_, 16);
            auto &style = widget.style();
            style.cornerRadius = 10;
            style.fillColor = Color::hex(fill);
            style.hoverFillColor = Color::hex(hoverFill);
            style.pressedFillColor = Color::hex(pressFill);
            style.borderColor = Color::hex(hovered && !primary && !danger ? accent : outline);
            style.textColor = Color::hex(labelInk);
            widget.setHovered(hovered);
            if (pressed)
                widget.handlePointerDown(b.x + 1, b.y + 1);
            widget.render(canvas_);
            continue;
        }
        if (toolButton)
            color = pressed ? selection : button.selected ? selection : hovered ? fieldColor : panel;
        canvas_.setFillColor(Color::hex(color));
        canvas_.fillRoundedRect(b.x, b.y, b.w, b.h, 8);
        if (swatch) {
            canvas_.setStrokeColor(Color::hex(hovered ? accent : border));
            canvas_.setLineWidth(1);
            canvas_.strokeRoundedRect(b.x, b.y, b.w, b.h, 8);
        }
        if (swatch && button.selected) {
            canvas_.setStrokeColor(Color::hex(accent));
            canvas_.setLineWidth(2);
            canvas_.strokeRoundedRect(b.x - 3, b.y - 3, b.w + 6, b.h + 6, 10);
        }
        if (toolButton) {
            Shape icon;
            icon.kind = button.action;
            icon.color = button.selected ? accent : textColor;
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
                    text(std::to_string(i + 1), b.x + 36, b.y + 45, muted);
        } else
            text(button.label, b.x + 10, b.y + 23, textColor);
    }
    const auto colorBox = layout.colorField();
    canvas_.setFillColor(Color::hex(view.editingColor ? selection : fieldColor));
    canvas_.fillRoundedRect(colorBox.x, colorBox.y, colorBox.w, colorBox.h, 5);
    if (view.editingColor && view.colorSelected) {
        canvas_.setFillColor(Color::hex(view.darkTheme ? 0x65528d : 0xd0bfff));
        canvas_.fillRoundedRect(colorBox.x + 7, colorBox.y + 5, 92, 24, 3);
    }
    text("#" + view.colorText + (view.editingColor ? "|" : ""), colorBox.x + 9, colorBox.y + 23, textColor);
    canvas_.setFillColor(Color::hex(view.color));
    canvas_.fillRoundedRect(colorBox.x + 117, colorBox.y + 8, 24, 18, 3);
    canvas_.setStrokeColor(Color::hex(border));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(colorBox.x + 117, colorBox.y + 8, 24, 18, 3);
    text("Width  " + std::to_string(int(view.strokeWidth)) + " px", 80, 735, textColor);
    text("Ctrl+Z undo   Ctrl+S export", 22, 872, muted);
    text(view.online ? (view.accessControlled ? "Key required / plain TCP" : "Open server / plain TCP")
                     : "Local / export to save",
         22, float(layout.height - 46), muted);
    text("Draw. Share. Think together.", 22, float(layout.height - 23), muted);
    const bool joining = view.connecting || (view.reconnect && !view.online);
    text(view.online ? "LIVE ROOM" : joining ? "JOINING ROOM" : "LOCAL STUDIO", 300, 41, accent);
    text(view.online || joining ? "Room / " + view.fields[2] : "Your local board / Export to save", 300, 72,
         textColor);
    text((view.online ? std::to_string(view.peers.size()) + " collaborators    |    " : "") +
             std::to_string(view.shapes.size()) + (view.online ? " room shapes" : " local shapes"),
         float(layout.width - 370), 72, muted);
    canvas_.setStrokeColor(Color::hex(border));
    canvas_.setLineWidth(1);
    canvas_.strokeRoundedRect(layout.left - 1, layout.top - 1, boardWidth * layout.scale + 2,
                              boardHeight * layout.scale + 2, 2);
    canvas_.save();
    canvas_.clipRect(layout.left, layout.top, boardWidth * layout.scale, boardHeight * layout.scale);
    canvas_.translate(layout.left, layout.top);
    canvas_.scale(layout.scale, layout.scale);
    canvas_.setFillColor(Color::hex(view.darkTheme ? 0x1b2433 : 0xffffff));
    canvas_.fillRect(0, 0, boardWidth, boardHeight);
    drawingBoard_ = true;
    canvas_.setFillColor(Color::hex(border));
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
        text(view.online ? (view.localShapeCount > 0
                                ? "Draw together. Use Add local to copy your offline sketch into this room."
                                : "Choose a tool and draw. Invite someone to the same room.")
             : joining   ? "Loading the room. Your local board is kept separate."
                         : "Start drawing here. No connection needed. Export to save your work.",
             300, 375, muted);
    }
    canvas_.restore();
    drawingBoard_ = false;
    text(view.status.substr(0, 110), 300, float(layout.height - 28),
         view.darkTheme ? 0xb6c9e7U
         : view.online  ? 0x2b8a3eU
                        : 0x9c36b5U);
    canvas_.endFrame();
}
} // namespace synccanvas
