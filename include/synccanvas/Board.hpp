#pragma once
#include "Protocol.hpp"

namespace synccanvas {
class Board {
  public:
    const std::vector<Shape> &shapes() const noexcept {
        return shapes_;
    }
    std::uint64_t revision() const noexcept {
        return revision_;
    }
    Shape add(Shape shape, const std::string &owner);
    std::string undo(const std::string &owner);
    Json document() const;
    void load(const Json &document);

  private:
    std::vector<Shape> shapes_;
    std::uint64_t revision_ = 0, nextId_ = 1;
};
} // namespace synccanvas
