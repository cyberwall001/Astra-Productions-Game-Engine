#include "astra/core/Log.h"

#include <deque>

#include "astra/core/Time.h"

namespace astra::log {

const char* LevelName(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Critical: return "CRITICAL";
    }
    return "INFO";
}

Sink& Sink::Instance() {
    static Sink sink;
    return sink;
}

void Sink::SetCapacity(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = capacity == 0 ? 1 : capacity;
    while (entries_.size() > capacity_) entries_.erase(entries_.begin());
}

void Sink::SetMinimumLevel(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minimum_ = level;
}

Level Sink::MinimumLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return minimum_;
}

void Sink::Add(Level level, std::string_view category, std::string message) {
    std::function<void(const Entry&)> handler;
    Entry entry;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (static_cast<int>(level) < static_cast<int>(minimum_)) return;
        entry.level = level;
        entry.category = std::string(category);
        entry.message = std::move(message);
        entry.timeUtc = time::NowUtc();
        entries_.push_back(entry);
        if (entries_.size() > capacity_) entries_.erase(entries_.begin());
        ++total_;
        handler = handler_;
    }
    if (handler) handler(entry);
}

std::vector<Entry> Sink::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void Sink::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

void Sink::SetHandler(std::function<void(const Entry&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handler_ = std::move(handler);
}

std::size_t Sink::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_;
}

void Write(Level level, std::string_view category, std::string message) {
    Sink::Instance().Add(level, category, std::move(message));
}

}  // namespace astra::log
