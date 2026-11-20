// github/GitHubModels.h - plain data structures returned by the GitHub client.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::github {

struct User {
    std::string login;
    std::string name;
    std::string company;
    std::string avatarUrl;
    int publicRepos = 0;
    bool valid = false;
};

struct Repo {
    std::int64_t id = 0;
    std::string name;
    std::string fullName;   // "owner/name"
    std::string owner;
    std::string description;
    std::string language;
    std::string defaultBranch = "main";
    std::string htmlUrl;
    std::vector<std::string> topics;
    bool isPrivate = false;
    bool isFork = false;
    bool isArchived = false;
    bool hasAstraProject = false;  // topic-based hint; verify with a probe
    bool probeChecked = false;
    int stars = 0;
    int openIssues = 0;
    std::int64_t updatedUtc = 0;
    std::int64_t pushedUtc = 0;
    int sizeKb = 0;
};

struct RateLimit {
    int limit = 60;
    int remaining = 60;
    std::int64_t resetUtc = 0;
};

struct RepoPage {
    std::vector<Repo> repos;
    bool hadError = false;
    std::string error;
    int nextPage = 0;  // 0 = no more pages
};

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
    int rateLimitRemaining = -1;
};

}  // namespace astra::github
