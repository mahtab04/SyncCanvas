#include "synccanvas/Board.hpp"
#include <algorithm>
#include <stdexcept>
namespace synccanvas {
Shape Board::add(Shape shape, const std::string &owner) {
    if (shapes_.size() >= maxShapes)
        throw std::length_error("Board is full (1500 shapes)");
    // Enforce the same validation for disk imports and network operations.
    shape = parseShape(shapeJson(shape));
    std::size_t points = shape.points.size();
    for (const auto &existing : shapes_)
        points += existing.points.size();
    if (points > 40000)
        throw std::length_error("Board point budget exceeded");
    shape.id = std::to_string(nextId_++);
    shape.owner = owner;
    shapes_.push_back(shape);
    ++revision_;
    return shape;
}
std::string Board::undo(const std::string &owner) {
    for (auto it = shapes_.rbegin(); it != shapes_.rend(); ++it) {
        if (it->owner == owner) {
            auto id = it->id;
            shapes_.erase(std::next(it).base());
            ++revision_;
            return id;
        }
    }
    return {};
}
Json Board::document() const {
    Json shapes = Json::array();
    for (const auto &shape : shapes_)
        shapes.push_back(shapeJson(shape));
    return {{"format", "synccanvas"}, {"version", protocolVersion}, {"shapes", shapes}};
}
void Board::load(const Json &document) {
    if (document.at("format") != "synccanvas" || document.at("version") != protocolVersion)
        throw std::invalid_argument("Unsupported board file");
    const auto &source = document.at("shapes");
    if (!source.is_array() || source.size() > maxShapes)
        throw std::length_error("Invalid board size");
    Board candidate;
    for (const auto &value : source)
        candidate.add(parseShape(value), "archive");
    *this = std::move(candidate); // Commit only after every shape validates.
}
} // namespace synccanvas
