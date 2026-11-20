// github/GitHubJson.h - parsing of GitHub REST payloads (kept transport-agnostic so it
// can be unit tested against recorded fixtures).
#pragma once

#include <string>
#include <string_view>

#include "astra/data/Json.h"
#include "astra/github/GitHubModels.h"

namespace astra::github {

// Parses one repo object. Returns false when required fields are missing.
bool ParseRepo(const json::Value& value, Repo& out);

// Parses an array of repos ("/user/repos" or "/orgs/{org}/repos" response body).
bool ParseRepoArray(std::string_view body, std::vector<Repo>& out, std::string* error);

// Parses the authenticated user ("/user").
bool ParseUser(std::string_view body, User& out, std::string* error);

// Parses the "next" URL out of a Link header: <https://...&page=2>; rel="next"
// Returns 0 when there is no next page.
int ParseNextPage(std::string_view linkHeader);

// Converts a GitHub ISO-8601 timestamp to unix seconds.
std::int64_t ParseTimestamp(const json::Value& value);

// Detects an Astra project from repo topics/name (cheap, no extra requests).
bool LooksLikeAstraProject(const Repo& repo);

// Builds the repo list URL for a page (1-based).
std::string RepoListUrl(int page, int perPage);

// Builds the URL used to probe for astra.project.json.
std::string ProjectProbeUrl(std::string_view fullName, std::string_view branch);

}  // namespace astra::github
