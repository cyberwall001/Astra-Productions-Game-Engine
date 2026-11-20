// core/Log.h - central log sink shared by the engine, the launcher and the editor UI.
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace astra::log {

enum class Level { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Critical = 5 };

struct Entry {
    Level level = Level::Info;
    std::string category;
    std::string message;
    std::int64_t timeUtc = 0;  // unix seconds
};

const char* LevelName(Level level);

// Thread safe ring buffer of the most recent entries (default 2048).
class Sink {
public:
    static Sink& Instance();

    void SetCapacity(std::size_t capacity);
    void SetMinimumLevel(Level level);
    Level MinimumLevel() const;

    void Add(Level level, std::string_view category, std::string message);

    std::vector<Entry> Snapshot() const;
    void Clear();

    // Called (on the logging thread) after every added entry; used by the UI to mark
    // the console panel dirty. Keep handlers cheap and non-reentrant.
    void SetHandler(std::function<void(const Entry&)> handler);

    std::size_t Count() const;

private:
    Sink() = default;

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::size_t capacity_ = 2048;
    Level minimum_ = Level::Trace;
    std::size_t total_ = 0;
    std::function<void(const Entry&)> handler_;
};

// Writes to the sink and, in debug builds, to the platform debug output.
void Write(Level level, std::string_view category, std::string message);

inline void Trace(std::string_view cat, std::string msg) { Write(Level::Trace, cat, std::move(msg)); }
inline void Debug(std::string_view cat, std::string msg) { Write(Level::Debug, cat, std::move(msg)); }
inline void Info(std::string_view cat, std::string msg) { Write(Level::Info, cat, std::move(msg)); }
inline void Warn(std::string_view cat, std::string msg) { Write(Level::Warn, cat, std::move(msg)); }
inline void Error(std::string_view cat, std::string msg) { Write(Level::Error, cat, std::move(msg)); }

}  // namespace astra::log
