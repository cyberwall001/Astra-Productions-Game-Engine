// github/ITransport.h - HTTP transport abstraction (WinHTTP in the shipped app,
// fixture-backed demo transport for offline use and tests).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "astra/github/GitHubModels.h"

namespace astra::github {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual HttpResult Get(const std::string& url, const std::vector<std::string>& headers,
                           int timeoutMs) = 0;
    virtual std::string Name() const = 0;
};

using TransportPtr = std::unique_ptr<ITransport>;

// Platform HTTPS transport (WinHTTP). Returns a transport that always fails with a
// descriptive error on unsupported platforms.
TransportPtr MakePlatformTransport();

// Fixture-backed transport used for the "demo workspace" when no token is configured.
TransportPtr MakeDemoTransport(std::string fixturePath);

}  // namespace astra::github
