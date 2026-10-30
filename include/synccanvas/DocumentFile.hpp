#pragma once
#include "Protocol.hpp"
#include <filesystem>

namespace synccanvas {
// Enforce the byte limit while reading, including files that grow during the read.
Json readDocumentFile(const std::filesystem::path &path, std::size_t limit = 16 * 1024 * 1024);
// Replace only after serialization, writing, flushing, and closing all succeed.
void writeDocumentFile(const std::filesystem::path &path, const Json &document, int indent = -1);
} // namespace synccanvas
