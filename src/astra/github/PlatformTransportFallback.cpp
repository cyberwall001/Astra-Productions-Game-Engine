// github/PlatformTransportFallback.cpp - non-Windows builds (engine tself-check and the
// UI preview tool) have no HTTPS stack; the hub reports this instead of crashing.
#include "astra/github/ITransport.h"

#if !defined(_WIN32)

namespace astra::github {
namespace {

class UnsupportedTransport final : public ITransport {
public:
    HttpResult Get(const std::string&, const std::vector<std::string>&, int) override {
        HttpResult result;
        result.error =
            "GitHub sync is only available in the Windows build of Astra Studio";
        return result;
    }
    std::string Name() const override { return "Unsupported"; }
};

}  // namespace

TransportPtr MakePlatformTransport() { return std::make_unique<UnsupportedTransport>(); }

}  // namespace astra::github

#endif  // !_WIN32
