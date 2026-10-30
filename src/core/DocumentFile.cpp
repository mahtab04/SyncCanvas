#include "synccanvas/DocumentFile.hpp"
#include <fstream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace synccanvas {
namespace {
class TemporaryDocument {
  public:
    explicit TemporaryDocument(const std::filesystem::path &target) {
        std::random_device random;
        for (int attempt = 0; attempt < 32; ++attempt) {
            auto candidate = target;
            candidate += ".tmp-" + std::to_string(random());
            // Exclusive directory creation avoids collisions between concurrent saves.
            if (std::filesystem::create_directory(candidate)) {
                directory_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("Could not create temporary document directory");
    }
    ~TemporaryDocument() {
        std::error_code ignored;
        std::filesystem::remove(directory_ / "document.json", ignored);
        std::filesystem::remove(directory_, ignored);
    }
    std::filesystem::path path() const {
        return directory_ / "document.json";
    }

  private:
    std::filesystem::path directory_;
};
} // namespace

Json readDocumentFile(const std::filesystem::path &path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open board file: " + path.string());
    std::string bytes;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - bytes.size())
            throw std::length_error("Board file exceeds size limit");
        bytes.append(buffer.data(), count);
    }
    if (input.bad() || !input.eof())
        throw std::runtime_error("Could not read board file: " + path.string());
    return parseDocument(bytes, limit);
}

void writeDocumentFile(const std::filesystem::path &path, const Json &document, int indent) {
    const auto bytes = document.dump(indent);
    TemporaryDocument temporary(path);
    const auto staging = temporary.path();
    {
        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(staging, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        output.close();
    }
#ifdef _WIN32
    if (!MoveFileExW(staging.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::filesystem::filesystem_error("Could not replace board file", staging, path,
                                                std::error_code(GetLastError(), std::system_category()));
#else
    std::filesystem::rename(staging, path);
#endif
}
} // namespace synccanvas
