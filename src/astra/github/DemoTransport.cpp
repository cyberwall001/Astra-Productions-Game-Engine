// github/DemoTransport.cpp - serves recorded GitHub responses so the project hub is
// fully usable (and testable) without a token or a network.
#include "astra/github/ITransport.h"

#include <chrono>
#include <thread>

#include "astra/core/FileIO.h"
#include "astra/data/Json.h"

namespace astra::github {
namespace {

class DemoTransport final : public ITransport {
public:
    explicit DemoTransport(std::string fixturePath) {
        std::string text;
        const bool read = fs::ReadTextFile(fixturePath, text);
        if (read) {
            std::string error;
            fixture_ = json::Value::ParseOrNull(text, &error);
        }
    }

    HttpResult Get(const std::string& url, const std::vector<std::string>&,
                   int) override {
        // A short delay keeps the loading states honest in the UI.
        std::this_thread::sleep_for(std::chrono::milliseconds(320));

        HttpResult result;
        result.ok = true;
        result.status = 200;
        result.rateLimitRemaining = 4873;

        if (url.find("/contents/astra.project.json") != std::string::npos) {
            // Probe: report whether the fixture declares a project for this repo.
            const auto slash = url.find("/repos/");
            std::string fullName;
            if (slash != std::string::npos) {
                const auto start = slash + 7;
                const auto end = url.find('/', start);
                fullName = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
            }
            const json::Value& projects = fixture_["projects"];
            const bool exists = projects.Has(fullName) && projects[fullName].AsBool();
            result.status = exists ? 200 : 404;
            result.ok = exists;
            result.body = exists ? "{\"name\":\"astra.project.json\",\"type\":\"file\"}"
                                 : "{\"message\":\"Not Found\"}";
            return result;
        }

        if (url.find("/user/repos") != std::string::npos) {
            // Match "&page=" so the per_page parameter is not mistaken for the page.
            int page = 1;
            const auto pagePos = url.find("&page=");
            if (pagePos != std::string::npos) page = std::atoi(url.c_str() + pagePos + 6);
            if (page > 1) {
                result.body = "[]";
            } else {
                result.body = fixture_["repos"].Dump(false);
            }
            return result;
        }

        if (url.find("/user") != std::string::npos) {
            result.body = fixture_["user"].Dump(false);
            return result;
        }

        result.ok = false;
        result.status = 404;
        result.body = "{\"message\":\"Not Found\"}";
        return result;
    }

    std::string Name() const override { return "Demo"; }

private:
    json::Value fixture_ = json::Value::MakeObject();
};

}  // namespace

TransportPtr MakeDemoTransport(std::string fixturePath) {
    return std::make_unique<DemoTransport>(std::move(fixturePath));
}

}  // namespace astra::github
