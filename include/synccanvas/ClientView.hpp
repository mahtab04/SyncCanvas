#pragma once
#include "Board.hpp"
#include <map>
#include <optional>
#include <vectorgl/canvas.hpp>
namespace synccanvas {
inline constexpr std::array<std::uint32_t, 12> inkColors{0x1e1e1e, 0xffffff, 0x868e96, 0xe03131,
                                                         0xf08c00, 0xf5c542, 0x2f9e44, 0x0c8599,
                                                         0x1971c2, 0x7048e8, 0xc2255c, 0xe859a8};
inline std::string colorHex(std::uint32_t color) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string value(6, '0');
    for (int i = 0; i < 6; ++i)
        value[i] = digits[(color >> ((5 - i) * 4)) & 15];
    return value;
}
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
    bool darkTheme = false, editingColor = false, colorSelected = false;
    bool clearArmed = false, clearing = false;
    std::string colorText = "1E1E1E";
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
    Rect colorField() const;
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
    bool darkTheme_ = false, drawingBoard_ = false;
    void text(const std::string &, float x, float y, std::uint32_t color = 0x495057);
    void shape(const Shape &);
    void strokeShape(const Shape &, std::uint32_t color, float width);
};
} // namespace synccanvas
