// render/Camera.h - the level viewport's orbit camera.
//
// Deliberately free of ImGui so headless tools and tests can drive it.
#pragma once

#include "astra/render/Math.h"

namespace astra::render {

struct Camera {
    Vec3 target{0.0f, 1.1f, 0.0f};
    float yawDeg = 46.0f;
    float pitchDeg = 20.0f;
    float distance = 13.5f;
    float fovDeg = 55.0f;

    static constexpr float kMinDistance = 1.2f;
    static constexpr float kMaxDistance = 260.0f;
    static constexpr float kMinPitch = -8.0f;
    static constexpr float kMaxPitch = 88.0f;

    // Unit vector from the target towards the eye.
    Vec3 OffsetDirection() const;
    Vec3 Position() const;

    // Orthonormal basis: forward points from the eye towards the target.
    void Basis(Vec3& forward, Vec3& right, Vec3& up) const;

    // Mouse-driven controls. Deltas are in pixels; wheel is in notches (usually +-1).
    void Orbit(float dxPixels, float dyPixels);
    void Pan(float dxPixels, float dyPixels, float viewportHeight);
    void Zoom(float wheelSteps);

    // Frames a sphere: used when focusing an actor or resetting the view.
    void Frame(const Vec3& center, float radius);
};

}  // namespace astra::render
