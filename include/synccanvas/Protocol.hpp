#pragma once
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace synccanvas {
using Json = nlohmann::json;
inline constexpr int protocolVersion = 1;
inline constexpr std::size_t maxFrameBytes = 512 * 1024;
inline constexpr std::size_t maxShapes = 1500;
inline constexpr std::size_t maxPoints = 512;
inline constexpr float boardWidth = 1200, boardHeight = 760;

struct DrawingTool {
    std::string_view kind, label;
    char shortcut;
};
inline constexpr std::array<DrawingTool, 6> drawingTools{{
    {"rect", "Rectangle", 'R'},
    {"diamond", "Diamond", 'D'},
    {"ellipse", "Ellipse", 'E'},
    {"arrow", "Arrow", 'A'},
    {"line", "Line", 'L'},
    {"pen", "Freehand", 'P'},
}};
bool validDrawingTool(std::string_view kind);

struct Point {
    float x = 0, y = 0;
};
struct Shape {
    std::string id, owner, kind = "pen";
    std::uint32_t color = 0x46dfbd;
    float width = 3;
    std::vector<Point> points;
};

// All externally supplied data crosses these validation functions.
bool validIdentifier(const std::string &value, std::size_t limit = 32);
Point parsePoint(const Json &value);
Shape parseShape(const Json &value);
Json shapeJson(const Shape &shape);
std::vector<std::uint8_t> encodeFrame(const Json &message);
std::uint32_t decodeLength(const std::array<std::uint8_t, 4> &header);
Json parseMessage(const std::string &bytes);
Json parseDocument(const std::string &bytes, std::size_t limit = 16 * 1024 * 1024);
} // namespace synccanvas
