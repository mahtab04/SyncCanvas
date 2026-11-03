#include "synccanvas/LocalAutosave.hpp"
#include "synccanvas/DocumentFile.hpp"
#include <algorithm>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace synccanvas {
struct LocalAutosave::Slot {
    std::filesystem::path directory;
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int handle = -1;
#endif
    ~Slot() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
#else
        if (handle >= 0)
            ::close(handle);
#endif
    }
    static std::unique_ptr<Slot> claim(const std::filesystem::path &directory) {
        auto slot = std::make_unique<Slot>();
        slot->directory = directory;
        const auto lock = directory / "owner.lock";
#ifdef _WIN32
        slot->handle = CreateFileW(lock.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (slot->handle == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION)
                return {};
            throw std::system_error(error, std::system_category(), "Cannot lock recovery slot");
        }
#else
        slot->handle = ::open(lock.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (slot->handle < 0)
            throw std::system_error(errno, std::generic_category(), "Cannot open recovery lock");
        if (::flock(slot->handle, LOCK_EX | LOCK_NB) != 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return {};
            throw std::system_error(errno, std::generic_category(), "Cannot lock recovery slot");
        }
#endif
        return slot;
    }
};
std::filesystem::path LocalAutosave::defaultDirectory() {
    std::filesystem::path root;
#ifdef _WIN32
    if (const char *state = std::getenv("LOCALAPPDATA"); state && *state)
        root = std::filesystem::path(state) / "SyncCanvas";
#else
    if (const char *state = std::getenv("XDG_STATE_HOME"); state && *state)
        root = std::filesystem::path(state) / "synccanvas";
    else if (const char *home = std::getenv("HOME"); home && *home)
        root = std::filesystem::path(home) / ".local/state/synccanvas";
#endif
    if (root.empty() || !root.is_absolute())
        throw std::runtime_error("No absolute per-user state directory available");
    return root / "recovery";
}
LocalAutosave::LocalAutosave(const std::filesystem::path &directory) {
    const bool created = std::filesystem::create_directories(directory);
#ifndef _WIN32
    if (created)
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all);
#else
    (void)created;
#endif
    struct Candidate {
        std::filesystem::path directory;
        std::filesystem::file_time_type modified;
    };
    std::vector<Candidate> candidates;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_symlink() || !entry.is_directory() ||
            !entry.path().filename().string().starts_with("board-"))
            continue;
        const auto file = entry.path() / "board.json";
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error) && !std::filesystem::is_symlink(file, error))
            candidates.push_back({entry.path(), std::filesystem::last_write_time(file)});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.modified == b.modified)
            return a.directory < b.directory;
        return a.modified > b.modified;
    });
    for (const auto &candidate : candidates) {
        auto slot = Slot::claim(candidate.directory);
        if (!slot)
            continue; // Another live window owns this recovery slot.
        try {
            Board archive;
            archive.load(readDocumentFile(candidate.directory / "board.json"));
            Board local;
            for (const auto &shape : archive.shapes())
                local.add(shape, "local");
            recovered_ = std::move(local);
            recoveredFile_ = true;
            slot_ = std::move(slot);
            break;
        } catch (const std::exception &) {
            warning_ = "Some recovery files could not be loaded and were preserved. Export important work.";
        }
    }
    if (!slot_) {
        std::random_device random;
        for (int i = 0; i < 64 && !slot_; ++i) {
            const auto path =
                directory / ("board-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (std::filesystem::create_directory(path)) {
#ifndef _WIN32
                std::filesystem::permissions(path, std::filesystem::perms::owner_all);
#endif
                slot_ = Slot::claim(path);
            }
        }
        if (!slot_)
            throw std::runtime_error("Cannot allocate a recovery slot");
    }
    worker_ = std::thread([this] { work(); });
}
LocalAutosave::~LocalAutosave() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        changed_.notify_all();
    }
    if (worker_.joinable())
        worker_.join();
    // Do not accumulate unused slots when an empty fresh window closes.
    std::error_code error;
    const auto directory = slot_->directory;
    const bool unused = !std::filesystem::exists(directory / "board.json", error) && !error;
    slot_.reset();
    if (unused) {
        std::filesystem::remove(directory / "owner.lock", error);
        std::filesystem::remove(directory, error);
    }
}
void LocalAutosave::submit(const Board &board) {
    std::lock_guard lock(mutex_);
    pending_ = board;
    ++submitted_;
    changed_.notify_all();
}
bool LocalAutosave::flush() {
    std::unique_lock lock(mutex_);
    const auto target = submitted_;
    if (saved_ >= target)
        return true;
    // Reset the attempt marker so a previously failed generation gets a fresh attempt.
    attempted_ = 0;
    immediate_ = true;
    changed_.notify_all();
    changed_.wait(lock, [this, target] { return saved_ >= target || attempted_ >= target; });
    return saved_ >= target;
}
std::string LocalAutosave::status() const {
    std::lock_guard lock(mutex_);
    if (failed_)
        return "Autosave failed / use Export";
    if (saved_ < submitted_)
        return "Saving local board...";
    return submitted_ || recoveredFile_ ? "Local board saved" : "Local autosave ready";
}
void LocalAutosave::work() {
    std::unique_lock lock(mutex_);
    for (;;) {
        changed_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
        if (stopping_ && !pending_)
            return;
        if (!stopping_ && !immediate_)
            changed_.wait_for(lock, std::chrono::seconds(2), [this] { return stopping_ || immediate_; });
        auto board = std::move(*pending_);
        pending_.reset();
        const auto generation = submitted_;
        immediate_ = false;
        lock.unlock();
        bool success = false;
        try {
            writeDocumentFile(slot_->directory / "board.json", board.document());
            success = true;
        } catch (const std::exception &) {
            // Atomic replacement retains the last successful recovery file.
        }
        lock.lock();
        attempted_ = generation;
        failed_ = !success;
        if (success)
            saved_ = generation;
        else if (!pending_)
            pending_ = std::move(board);
        changed_.notify_all();
        if (stopping_ && (!pending_ || submitted_ == generation))
            return;
    }
}
} // namespace synccanvas
