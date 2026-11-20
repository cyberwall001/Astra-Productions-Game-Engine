#include "astra/github/GitHubJson.h"

#include <cstdio>

#include "astra/core/Strings.h"
#include "astra/core/Time.h"

namespace astra::github {

std::int64_t ParseTimestamp(const json::Value& value) {
    const std::string text = value.AsString();
    if (text.empty()) return 0;
    return time::ParseIso8601(text);
}

bool ParseRepo(const json::Value& value, Repo& out) {
    if (!value.IsObject()) return false;
    const std::string name = value["name"].AsString();
    if (name.empty()) return false;

    out.name = name;
    out.id = value["id"].AsInt();
    out.fullName = value["full_name"].AsString();
    if (out.fullName.empty()) {
        out.fullName = value["owner"]["login"].AsString() + "/" + name;
    }
    out.owner = value["owner"]["login"].AsString();
    out.description = value["description"].AsString();
    out.language = value["language"].AsString();
    out.defaultBranch = value["default_branch"].AsString("main");
    out.htmlUrl = value["html_url"].AsString();
    out.isPrivate = value["private"].AsBool();
    out.isFork = value["fork"].AsBool();
    out.isArchived = value["archived"].AsBool();
    out.stars = static_cast<int>(value["stargazers_count"].AsInt());
    out.openIssues = static_cast<int>(value["open_issues_count"].AsInt());
    out.sizeKb = static_cast<int>(value["size"].AsInt());
    out.updatedUtc = ParseTimestamp(value["updated_at"]);
    out.pushedUtc = ParseTimestamp(value["pushed_at"]);

    for (const auto& topic : value["topics"].Elements()) {
        out.topics.push_back(topic.AsString());
    }
    out.hasAstraProject = LooksLikeAstraProject(out);
    return true;
}

bool ParseRepoArray(std::string_view body, std::vector<Repo>& out, std::string* error) {
    out.clear();
    json::Value root;
    std::string parseError;
    if (!json::Value::Parse(body, root, &parseError)) {
        if (error) *error = "GitHub response was not valid JSON: " + parseError;
        return false;
    }
    if (root.IsObject() && root.Has("message")) {
        // GitHub error envelope: {"message": "Bad credentials", ...}
        if (error) *error = root["message"].AsString("GitHub rejected the request");
        return false;
    }
    if (!root.IsArray()) {
        if (error) *error = "Expected a list of repositories";
        return false;
    }
    for (const auto& item : root.Elements()) {
        Repo repo;
        if (ParseRepo(item, repo)) out.push_back(std::move(repo));
    }
    return true;
}

bool ParseUser(std::string_view body, User& out, std::string* error) {
    json::Value root;
    std::string parseError;
    if (!json::Value::Parse(body, root, &parseError)) {
        if (error) *error = "GitHub response was not valid JSON: " + parseError;
        return false;
    }
    if (root.Has("message") && !root.Has("login")) {
        if (error) *error = root["message"].AsString("GitHub rejected the token");
        return false;
    }
    out.login = root["login"].AsString();
    if (out.login.empty()) {
        if (error) *error = "GitHub did not return a login";
        return false;
    }
    out.name = root["name"].AsString();
    out.company = root["company"].AsString();
    out.avatarUrl = root["avatar_url"].AsString();
    out.publicRepos = static_cast<int>(root["public_repos"].AsInt());
    out.valid = true;
    return true;
}

int ParseNextPage(std::string_view linkHeader) {
    if (linkHeader.empty()) return 0;
    // Look for: rel="next" inside a <...page=N...> section.
    std::size_t search = 0;
    while (true) {
        const std::size_t next = linkHeader.find("rel=\"next\"", search);
        if (next == std::string_view::npos) return 0;
        const std::size_t open = linkHeader.rfind('<', next);
        const std::size_t close = linkHeader.rfind('>', next);
        if (open != std::string_view::npos && close != std::string_view::npos && close > open) {
            const std::string url(linkHeader.substr(open + 1, close - open - 1));
            const std::size_t page = url.find("page=");
            if (page != std::string::npos) {
                return std::atoi(url.c_str() + page + 5);
            }
        }
        search = next + 10;
    }
}

bool LooksLikeAstraProject(const Repo& repo) {
    for (const auto& topic : repo.topics) {
        if (str::EqualsIgnoreCase(topic, "astra-engine") || str::EqualsIgnoreCase(topic, "astra")) {
            return true;
        }
    }
    if (str::Contains(str::ToLower(repo.name), "astra")) return true;
    if (str::Contains(str::ToLower(repo.description), "astra engine")) return true;
    return false;
}

std::string RepoListUrl(int page, int perPage) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "https://api.github.com/user/repos?per_page=%d&page=%d&sort=updated"
                  "&affiliation=owner,collaborator,organization_member",
                  perPage, page);
    return buffer;
}

std::string ProjectProbeUrl(std::string_view fullName, std::string_view branch) {
    return "https://api.github.com/repos/" + std::string(fullName) + "/contents/astra.project.json?ref=" +
           std::string(branch.empty() ? "main" : branch);
}

}  // namespace astra::github
