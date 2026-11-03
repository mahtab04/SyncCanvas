#pragma once
#include "Board.hpp"
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace synccanvas {
class LocalAutosave {
  public:
    static std::filesystem::path defaultDirectory();
    explicit LocalAutosave(const std::filesystem::path &directory);
    ~LocalAutosave();
    const Board &recoveredBoard() const {
        return recovered_;
    }
    bool recovered() const {
        return recoveredFile_;
    }
    const std::string &recoveryWarning() const {
        return warning_;
    }
    void submit(const Board &board);
    // Wait for one immediate attempt at the latest submitted board; failures remain retryable.
    bool flush();
    std::string status() const;

  private:
    struct Slot;
    std::unique_ptr<Slot> slot_;
    Board recovered_;
    bool recoveredFile_ = false;
    std::string warning_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::optional<Board> pending_;
    std::uint64_t submitted_ = 0, saved_ = 0, attempted_ = 0;
    bool stopping_ = false, immediate_ = false, failed_ = false;
    std::thread worker_;
    void work();
};
} // namespace synccanvas
