#include "astra/render/ViewportTypes.h"

namespace astra::render {

const char* ViewModeName(ViewMode mode) {
    switch (mode) {
        case ViewMode::Unlit:
            return "Unlit";
        case ViewMode::Wireframe:
            return "Wireframe";
        case ViewMode::Normals:
            return "Normals";
        case ViewMode::Shaded:
        default:
            return "Shaded";
    }
}

const char* const* ViewModeNames(int& count) {
    static const char* const kNames[] = {"Shaded", "Unlit", "Wireframe", "Normals"};
    count = 4;
    return kNames;
}

}  // namespace astra::render
