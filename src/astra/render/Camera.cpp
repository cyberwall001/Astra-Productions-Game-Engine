#include "astra/render/Camera.h"

#include <algorithm>
#include <cmath>

namespace astra::render {

Vec3 Camera::OffsetDirection() const {
    const float yaw = Radians(yawDeg);
    const float pitch = Radians(pitchDeg);
    const float horizontal = std::cos(pitch);
    return Vec3(horizontal * std::sin(yaw), std::sin(pitch), horizontal * std::cos(yaw));
}

Vec3 Camera::Position() const { return target + OffsetDirection() * distance; }

void Camera::Basis(Vec3& forward, Vec3& right, Vec3& up) const {
    forward = Normalize(target - Position());
    const Vec3 worldUp(0.0f, 1.0f, 0.0f);
    right = Normalize(Cross(forward, worldUp));
    if (Length(Cross(forward, worldUp)) < 1e-5f) right = Vec3(1.0f, 0.0f, 0.0f);
    up = Cross(right, forward);
}

void Camera::Orbit(float dxPixels, float dyPixels) {
    yawDeg -= dxPixels * 0.35f;
    pitchDeg += dyPixels * 0.28f;
    if (yawDeg > 360.0f) yawDeg -= 360.0f;
    if (yawDeg < -360.0f) yawDeg += 360.0f;
    pitchDeg = std::clamp(pitchDeg, kMinPitch, kMaxPitch);
}

void Camera::Pan(float dxPixels, float dyPixels, float viewportHeight) {
    Vec3 forward, right, up;
    Basis(forward, right, up);
    const float halfHeight = std::tan(Radians(fovDeg) * 0.5f);
    const float perPixel = (2.0f * halfHeight * distance) / std::max(1.0f, viewportHeight);
    target += right * (-dxPixels * perPixel);
    target += up * (dyPixels * perPixel);
}

void Camera::Zoom(float wheelSteps) {
    distance *= std::exp(-wheelSteps * 0.12f);
    distance = std::clamp(distance, kMinDistance, kMaxDistance);
}

void Camera::Frame(const Vec3& center, float radius) {
    target = center;
    const float safeRadius = std::max(0.5f, radius);
    distance = safeRadius / std::sin(Radians(fovDeg) * 0.5f) * 1.35f;
    distance = std::clamp(distance, kMinDistance, kMaxDistance);
}

}  // namespace astra::render
