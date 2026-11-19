#include "astra/github/GitHubClient.h"

#include <utility>

#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/github/GitHubJson.h"

namespace astra::github {

HttpResult GitHubClient::Get(const std::string& url) {
    std::vector<std::string> headers = {
        "User-Agent: AstraStudio/0.1 (Astra Productions)",
        "Accept: application/vnd.github+json",
        "X-GitHub-Api-Version: 2022-11-28",
    };
    if (!token_.empty()) headers.push_back("Authorization: Bearer " + token_);
    return transport_.Get(url, headers, 15000);
}

bool GitHubClient::FetchUser(User& out, std::string& error) {
    const HttpResult response = Get("https://api.github.com/user");
    if (!response.ok) {
        error = response.status == 401 ? "GitHub rejected this token" : response.error;
        return false;
    }
    return ParseUser(response.body, out, &error);
}

bool GitHubClient::FetchRepos(std::vector<Repo>& out, std::string& error, int maxPages,
                              RateLimit* rateLimit) {
    out.clear();
    for (int page = 1; page <= maxPages; ++page) {
        const HttpResult response = Get(RepoListUrl(page, 100));
        if (!response.ok) {
            error = response.status == 401
                        ? "GitHub rejected this token"
                        : (response.status == 403 ? "GitHub rate limit reached" : response.error);
            return !out.empty();  // keep partial results
        }
        if (rateLimit && response.rateLimitRemaining >= 0) {
            rateLimit->remaining = response.rateLimitRemaining;
        }
        std::string parseError;
        std::vector<Repo> pageRepos;
        if (!ParseRepoArray(response.body, pageRepos, &parseError)) {
            error = parseError;
            return !out.empty();
        }
        if (pageRepos.empty()) break;
        out.insert(out.end(), pageRepos.begin(), pageRepos.end());
        if (pageRepos.size() < 100) break;
    }
    return true;
}

bool GitHubClient::ProbeProject(const Repo& repo, bool& found, std::string& error) {
    const HttpResult response = Get(ProjectProbeUrl(repo.fullName, repo.defaultBranch));
    if (response.ok) {
        found = true;
        return true;
    }
    if (response.status == 404) {
        found = false;
        return true;
    }
    error = response.error.empty() ? "Probe failed" : response.error;
    return false;
}

// ---------------------------------------------------------------------------
// RepoFetcher
// ---------------------------------------------------------------------------

RepoFetcher::RepoFetcher() : RepoFetcher(paths::Asset("demo/github_demo.json")) {}

RepoFetcher::RepoFetcher(std::string demoFixturePath) : demoPath_(std::move(demoFixturePath)) {
    worker_ = std::thread([this] { WorkerMain(); });
}

RepoFetcher::~RepoFetcher() { Shutdown(); }

std::string RepoFetcher::TakeDemoPath() const { return demoPath_; }

void RepoFetcher::Start(std::string token, bool allowDemo) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.clear();
        Job job;
        job.kind = JobKind::Sync;
        job.token = std::move(token);
        job.demo = allowDemo && job.token.empty();
        jobs_.push_back(std::move(job));

        activeToken_ = token;
        snapshot_.state = FetchState::Loading;
        snapshot_.error.clear();
        snapshot_.demo = allowDemo && token.empty();
    }
    wake_.notify_one();
}

void RepoFetcher::RequestProbe(Repo repo) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Job job;
        job.kind = JobKind::Probe;
        job.repo = std::move(repo);
        jobs_.push_back(std::move(job));
    }
    wake_.notify_one();
}

bool RepoFetcher::Poll() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasPending_) return false;
    snapshot_ = std::move(pending_);
    hasPending_ = false;
    return true;
}

RepoSnapshot RepoFetcher::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool RepoFetcher::IsBusy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_.state == FetchState::Loading || busy_;
}

std::map<std::string, bool> RepoFetcher::ProbeResults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return probes_;
}

void RepoFetcher::Publish(std::function<void(RepoSnapshot&)> mutate) {
    std::lock_guard<std::mutex> lock(mutex_);
    mutate(pending_);
    hasPending_ = true;
}

void RepoFetcher::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) return;
        stop_ = true;
        jobs_.clear();
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void RepoFetcher::WorkerMain() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
            busy_ = true;
        }

        if (job.kind == JobKind::Sync) {
            RunSync(job);
        } else {
            RunProbe(job);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            busy_ = false;
        }
    }
}

void RepoFetcher::RunSync(const Job& job) {
    TransportPtr transport = job.demo ? MakeDemoTransport(TakeDemoPath())
                                      : MakePlatformTransport();
    GitHubClient client(*transport);
    client.SetToken(job.token);

    User user;
    std::string error;
    const bool userOk = client.FetchUser(user, error);
    if (!userOk) {
        log::Warn("github", "user lookup failed: " + error);
    }

    std::vector<Repo> repos;
    RateLimit rate;
    std::string repoError;
    const bool reposOk = client.FetchRepos(repos, repoError, 4, &rate);

    // Mark topic-detected projects; probes refine this later.
    for (auto& repo : repos) repo.hasAstraProject = LooksLikeAstraProject(repo);

    std::size_t repoCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = RepoSnapshot{};
        pending_.demo = job.demo;
        pending_.user = user;
        pending_.repos = std::move(repos);
        pending_.rate = rate;
        pending_.finishedAtMs = time::NowMonotonicMs();
        if (!reposOk) {
            pending_.state = FetchState::Failed;
            pending_.error = repoError;
        } else {
            pending_.state = FetchState::Ready;
        }
        if (!userOk && pending_.error.empty()) {
            pending_.state = FetchState::Failed;
            pending_.error = error;
        }
        hasPending_ = true;
        repoCount = pending_.repos.size();
    }
    log::Info("github", str::Format("synced %zu repositories via %s", repoCount,
                                    transport->Name().c_str()));
}

void RepoFetcher::RunProbe(const Job& job) {
    bool demo = false;
    std::string token;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        demo = snapshot_.demo;
        token = activeToken_;
    }

    TransportPtr transport = demo ? MakeDemoTransport(TakeDemoPath()) : MakePlatformTransport();
    GitHubClient client(*transport);
    client.SetToken(token);

    bool found = false;
    std::string error;
    if (!client.ProbeProject(job.repo, found, error)) {
        log::Warn("github", "project probe failed for " + job.repo.fullName + ": " + error);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    probes_[job.repo.fullName] = found;
    for (auto& repo : pending_.repos) {
        if (repo.fullName == job.repo.fullName) {
            repo.hasAstraProject = found;
            repo.probeChecked = true;
        }
    }
    for (auto& repo : snapshot_.repos) {
        if (repo.fullName == job.repo.fullName) {
            repo.hasAstraProject = found;
            repo.probeChecked = true;
        }
    }
    hasPending_ = true;
}

}  // namespace astra::github
