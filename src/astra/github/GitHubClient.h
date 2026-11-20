// github/GitHubClient.h - synchronous GitHub REST calls plus the threaded fetcher the
// launcher UI drives (so no network work ever blocks a frame).
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include "astra/github/GitHubModels.h"
#include "astra/github/ITransport.h"

namespace astra::github {

class GitHubClient {
public:
    explicit GitHubClient(ITransport& transport) : transport_(transport) {}

    void SetToken(std::string token) { token_ = std::move(token); }
    const std::string& Token() const { return token_; }

    // GET with the standard GitHub headers + bearer token.
    HttpResult Get(const std::string& url);

    bool FetchUser(User& out, std::string& error);
    bool FetchRepos(std::vector<Repo>& out, std::string& error, int maxPages = 4,
                    RateLimit* rateLimit = nullptr);
    // Returns true when the repo contains astra.project.json.
    bool ProbeProject(const Repo& repo, bool& found, std::string& error);

private:
    ITransport& transport_;
    std::string token_;
};

enum class FetchState { Idle, Loading, Ready, Failed, NoToken };

// Immutable snapshot handed to the UI each frame.
struct RepoSnapshot {
    FetchState state = FetchState::Idle;
    std::string error;
    User user;
    std::vector<Repo> repos;
    RateLimit rate;
    bool demo = false;
    double finishedAtMs = 0.0;
};

// Owns one background thread that performs fetches and project probes.
class RepoFetcher {
public:
    RepoFetcher();  // demo fixture path resolved from the asset root
    explicit RepoFetcher(std::string demoFixturePath);
    ~RepoFetcher();

    RepoFetcher(const RepoFetcher&) = delete;
    RepoFetcher& operator=(const RepoFetcher&) = delete;

    // Starts (or restarts) a repository sync. Empty token => demo data when `allowDemo`.
    void Start(std::string token, bool allowDemo);

    // Queues a check for astra.project.json on a specific repo.
    void RequestProbe(Repo repo);

    // Non-blocking; call once per frame. Returns true when new data arrived.
    bool Poll();

    RepoSnapshot Snapshot() const;
    bool IsBusy() const;

    // Probe results keyed by full name ("owner/name").
    std::map<std::string, bool> ProbeResults() const;

    void Shutdown();

private:
    enum class JobKind { Sync, Probe };

    struct Job {
        JobKind kind = JobKind::Sync;
        std::string token;
        bool demo = false;
        Repo repo;
    };

    void WorkerMain();
    void RunSync(const Job& job);
    void RunProbe(const Job& job);
    void Publish(std::function<void(RepoSnapshot&)> mutate);
    std::string TakeDemoPath() const;

    std::string demoPath_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    RepoSnapshot snapshot_;
    RepoSnapshot pending_;
    bool hasPending_ = false;
    std::map<std::string, bool> probes_;
    std::string activeToken_;
    bool stop_ = false;
    bool busy_ = false;
    std::thread worker_;
};

}  // namespace astra::github
