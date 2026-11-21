// render/ViewportTypes.h - viewport state that the app object owns and the panels edit.
//
// Free of ImGui so AstraApp.h can hold it without dragging the UI toolkit into every
// translation unit that includes the app header.
#pragma once

namespace astra::render {

enum class ViewMode { Shaded = 0, Unlit, Wireframe, Normals };
const char* ViewModeName(ViewMode mode);
const char* const* ViewModeNames(int& count);

struct Options {
    ViewMode mode = ViewMode::Shaded;
    bool showGrid = true;
    bool showShadows = true;
    bool showGizmos = true;
    bool showActorIcons = true;
};

struct Stats {
    int actors = 0;
    int drawnActors = 0;
    int triangles = 0;
    int gridLines = 0;
    float milliseconds = 0.0f;
};

}  // namespace astra::render
