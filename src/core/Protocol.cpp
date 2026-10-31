#include "synccanvas/Protocol.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace synccanvas {
namespace {
float finiteNumber(const Json &value, float low, float high) {
    if (!value.is_number())
        throw std::invalid_argument("Expected a number");
    const auto number = value.get<double>();
    if (!std::isfinite(number) || number < low || number > high)
        throw std::invalid_argument("Number outside permitted range");
    return static_cast<float>(number);
}
} // namespace
bool validDrawingTool(std::string_view kind) {
    for (const auto &tool : drawingTools)
        if (tool.kind == kind)
            return true;
    return false;
}
bool validIdentifier(const std::string &value, std::size_t limit) {
    if (value.empty() || value.size() > limit)
        return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
              c == '-'))
            return false;
    return true;
}
Point parsePoint(const Json &value) {
    if (!value.is_array() || value.size() != 2)
        throw std::invalid_argument("Invalid point");
    return {finiteNumber(value[0], 0, boardWidth), finiteNumber(value[1], 0, boardHeight)};
}
Point constrainEndpoint(Point start, Point end, std::string_view kind) {
    if (kind == "pen")
        return end;
    const float dx = end.x - start.x, dy = end.y - start.y;
    if (kind == "line" || kind == "arrow") {
        const float step = std::numbers::pi_v<float> / 4;
        const float angle = std::round(std::atan2(dy, dx) / step) * step;
        const float ux = std::cos(angle), uy = std::sin(angle);
        float length = std::hypot(dx, dy);
        if (std::abs(ux) > .0001f)
            length = std::min(length, (ux > 0 ? boardWidth - start.x : start.x) / std::abs(ux));
        if (std::abs(uy) > .0001f)
            length = std::min(length, (uy > 0 ? boardHeight - start.y : start.y) / std::abs(uy));
        return {std::clamp(start.x + ux * length, 0.f, boardWidth),
                std::clamp(start.y + uy * length, 0.f, boardHeight)};
    }
    const float side =
        std::min({std::max(std::abs(dx), std::abs(dy)), dx < 0 ? start.x : boardWidth - start.x,
                  dy < 0 ? start.y : boardHeight - start.y});
    return {start.x + (dx < 0 ? -side : side), start.y + (dy < 0 ? -side : side)};
}
Shape parseShape(const Json &value) {
    Shape result;
    result.kind = value.at("kind").get<std::string>();
    if (!validDrawingTool(result.kind))
        throw std::invalid_argument("Unknown drawing tool");
    const auto &color = value.at("color");
    if (!color.is_number_integer() || color.get<std::int64_t>() < 0 || color.get<std::uint64_t>() > 0xffffff)
        throw std::invalid_argument("Invalid color");
    result.color = color.get<std::uint32_t>();
    result.width = finiteNumber(value.at("width"), 1, 16);
    const auto &points = value.at("points");
    if (!points.is_array() || points.size() < 2 || points.size() > maxPoints)
        throw std::invalid_argument("Invalid point count");
    if (result.kind != "pen" && points.size() != 2)
        throw std::invalid_argument("Expected two corners");
    for (const auto &point : points)
        result.points.push_back(parsePoint(point));
    // Identity is never accepted from a drawing request. The server assigns it.
    return result;
}
Json shapeJson(const Shape &shape) {
    Json points = Json::array();
    for (auto p : shape.points)
        points.push_back({p.x, p.y});
    return {{"id", shape.id},       {"owner", shape.owner}, {"kind", shape.kind},
            {"color", shape.color}, {"width", shape.width}, {"points", points}};
}
std::vector<std::uint8_t> encodeFrame(const Json &message) {
    const auto body = message.dump();
    if (body.empty() || body.size() > maxFrameBytes)
        throw std::length_error("Message too large");
    const auto length = static_cast<std::uint32_t>(body.size());
    std::vector<std::uint8_t> frame{
        static_cast<std::uint8_t>(length >> 24), static_cast<std::uint8_t>(length >> 16),
        static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(length)};
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}
std::uint32_t decodeLength(const std::array<std::uint8_t, 4> &header) {
    std::uint32_t length = 0;
    for (auto byte : header)
        length = (length << 8) | byte;
    if (length == 0 || length > maxFrameBytes)
        throw std::length_error("Invalid frame length");
    return length;
}
Json parseDocument(const std::string &bytes, std::size_t limit) {
    if (bytes.empty() || bytes.size() > limit)
        throw std::length_error("Invalid message size");
    // Bound nesting before the JSON parser allocates a deeply recursive document.
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : bytes) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > 16)
                throw std::invalid_argument("JSON nesting limit");
        } else if (c == '}' || c == ']')
            --depth;
    }
    return Json::parse(bytes);
}
Json parseMessage(const std::string &bytes) {
    auto message = parseDocument(bytes, maxFrameBytes);
    if (!message.is_object())
        throw std::invalid_argument("Expected a message object");
    const auto type = message.at("type").get<std::string>();
    if (!validIdentifier(type, 24))
        throw std::invalid_argument("Invalid message type");
    return message;
}
} // namespace synccanvas
