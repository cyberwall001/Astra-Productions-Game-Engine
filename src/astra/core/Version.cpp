#include "astra/core/Version.h"

namespace astra {

std::string FullVersionString() {
    return std::string(kEngineVersion) + "-" + kBuildChannel + " (" + kEngineCodename + ")";
}

}  // namespace astra
