#pragma once
#include "Board.hpp"
#include <map>
#include <optional>
#include <vectorgl/canvas.hpp>
namespace synccanvas {
inline constexpr std::array<std::uint32_t, 5> inkColors{0x1e1e1e, 0xe03131, 0x2f9e44, 0x1971c2, 0xf08c00};
struct RemoteCursor {
    std::string name;
    Point position;
    bool visible = false;
    std::optional<Shape> preview;
};
struct ClientView {
    std::array<std::string, 5> fields{"127.0.0.1", "8765", "", "designer", ""};
    int activeField = -1;
    bool fieldSelected = false, showGrid = true, accessControlled = false;
    std::string tool = "pen", status = "Draw locally now. Export to save, or join a room to collaborate.",
                selfId;
    std::size_t localShapeCount = 0;
    std::string hoveredTool;
    std::uint32_t color = inkColors[0];
    float strokeWidth = 3;
    bool online = false, connecting = false, reconnect = false;
    std::uint64_t revision = 0;
    std::vector<Shape> shapes, snapshot;
    std::map<std::string, RemoteCursor> peers;
    std::optional<Shape> draft;
};
struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};
struct Button {
    Rect bounds;
    std::string action, label;
    bool selected = false;
};
class ClientLayout {
  public:
    void resize(int width, int height);
    Point toBoard(float x, float y) const;
    bool onBoard(float x, float y) const;
    Rect field(int index) const;
    Rect toolbar() const;
    std::vector<Button> buttons(const ClientView &view) const;
    float left = 290, top = 110, scale = 1;
    int width = 1440, height = 940;
};
class Renderer {
  public:
    bool init();
    void destroy();
    void draw(const ClientView &, const ClientLayout &);

  private:
    vectorgl::Canvas canvas_;
    bool hasFont_ = false;
    void text(const std::string &, float x, float y, std::uint32_t color = 0x495057);
    void shape(const Shape &);
};
} // namespace synccanvas
