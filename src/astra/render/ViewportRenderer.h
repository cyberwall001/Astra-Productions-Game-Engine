// render/ViewportRenderer.h - the CPU level viewport.
//
// There is no GPU pass in this milestone, so the viewport rasterises straight into the
// ImGui draw list: geometry is transformed, lit, depth sorted and emitted as triangles.
// That keeps the editor identical on the GLFW build and in the headless preview tool,
// and it is fast enough for the sizes these levels reach.
#pragma once

#include <string>

#include "astra/render/Camera.h"
#include "astra/render/ViewportTypes.h"
#include "astra/scene/Scene.h"
#include "imgui.h"

namespace astra::render {

// Screen-space rectangle. Deliberately not ImRect: that lives in imgui_internal.h and
// the app headers should not depend on ImGui internals.
struct Rect {
    ImVec2 min{0.0f, 0.0f};
    ImVec2 max{0.0f, 0.0f};

    float Width() const { return max.x - min.x; }
    float Height() const { return max.y - min.y; }
    bool Contains(const ImVec2& point) const {
        return point.x >= min.x && point.y >= min.y && point.x <= max.x && point.y <= max.y;
    }
};

// Axis-aligned bounds of an actor in world space (mesh size folded in).
bool ActorBounds(const scene::Entity& entity, Vec3& center, Vec3& extents);

// Draws the scene into `rect` on `dl`. The caller owns the clip rect and the clip stack.
Stats Draw(ImDrawList* dl, const Rect& rect, const scene::Scene& scene, const Camera& camera,
           const Options& options, scene::EntityId selected, float time);

// Screen-space picking used by the viewport's left click. Returns kInvalidEntity on a miss.
scene::EntityId Pick(const Rect& rect, const scene::Scene& scene, const Camera& camera,
                     ImVec2 mouse);

// Frames the selected actor, or the whole level when nothing is selected.
void FrameSelection(Camera& camera, const scene::Scene& scene, scene::EntityId selected);

}  // namespace astra::render
