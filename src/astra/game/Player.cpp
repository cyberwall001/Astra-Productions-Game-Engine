#include "astra/game/Player.h"

#include <algorithm>
#include <cmath>

namespace astra::game {
namespace {

using scene::Vec3;

constexpr float kPi = 3.14159265358979f;

Vec3 Normalize(const Vec3& v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-6f) return Vec3(0.0f, 0.0f, 1.0f);
    return Vec3(v.x / length, v.y / length, v.z / length);
}

}  // namespace

void Player::SpawnAt(const Vec3& groundPosition, float yaw) {
    position = Vec3(groundPosition.x, groundPosition.y + eyeHeight, groundPosition.z);
    yawDeg = yaw;
    pitchDeg = -4.0f;
    velocityY = 0.0f;
    onGround = true;
}

void Player::Reset() {
    position = Vec3(0.0f, eyeHeight, 6.0f);
    yawDeg = 180.0f;
    pitchDeg = -4.0f;
    velocityY = 0.0f;
    onGround = true;
}

Vec3 Player::Eye() const { return position; }

Vec3 Player::Forward() const {
    const float yaw = yawDeg * kPi / 180.0f;
    const float pitch = pitchDeg * kPi / 180.0f;
    const float horizontal = std::cos(pitch);
    return Vec3(horizontal * std::sin(yaw), std::sin(pitch), horizontal * std::cos(yaw));
}

Vec3 Player::Right() const {
    const float yaw = yawDeg * kPi / 180.0f;
    return Vec3(std::cos(yaw), 0.0f, -std::sin(yaw));
}

void Player::Update(float dt, const PlayerInput& input) {
    dt = std::clamp(dt, 0.0f, 0.1f);

    yawDeg -= input.lookDeltaX * mouseSensitivity;
    pitchDeg = std::clamp(pitchDeg - input.lookDeltaY * mouseSensitivity, -88.0f, 88.0f);
    if (yawDeg > 360.0f) yawDeg -= 360.0f;
    if (yawDeg < -360.0f) yawDeg += 360.0f;

    // Movement is camera relative but stays on the horizontal plane.
    const Vec3 flatForward = Normalize(Vec3(Forward().x, 0.0f, Forward().z));
    const Vec3 flatRight = Right();
    Vec3 wish(0.0f, 0.0f, 0.0f);
    if (input.forward) {
        wish.x += flatForward.x;
        wish.z += flatForward.z;
    }
    if (input.back) {
        wish.x -= flatForward.x;
        wish.z -= flatForward.z;
    }
    if (input.right) {
        wish.x += flatRight.x;
        wish.z += flatRight.z;
    }
    if (input.left) {
        wish.x -= flatRight.x;
        wish.z -= flatRight.z;
    }

    const float length = std::sqrt(wish.x * wish.x + wish.z * wish.z);
    const float speed = input.sprint ? sprintSpeed : walkSpeed;
    if (length > 1e-4f) {
        wish = Vec3(wish.x / length, 0.0f, wish.z / length);
        position = Vec3(position.x + wish.x * speed * dt, position.y,
                        position.z + wish.z * speed * dt);
    }

    if (input.jump && onGround) {
        velocityY = jumpSpeed;
        onGround = false;
    }

    if (!onGround) {
        velocityY -= gravity * dt;
        position.y += velocityY * dt;
        if (position.y <= eyeHeight) {
            position.y = eyeHeight;
            velocityY = 0.0f;
            onGround = true;
        }
    }
}

render::Camera Player::ViewCamera(float fovDeg) const {
    // The renderer's camera orbits a target; for first person the eye is the orbit position
    // and the target sits one metre in front, so the projection matches the view exactly.
    const Vec3 forward = Forward();
    render::Camera camera;
    camera.target = Vec3(position.x + forward.x, position.y + forward.y, position.z + forward.z);
    camera.yawDeg = yawDeg + 180.0f;
    camera.pitchDeg = -pitchDeg;
    camera.distance = 1.0f;
    camera.fovDeg = fovDeg;
    return camera;
}

bool SpawnFromLevel(Player& player, const scene::Scene& scene) {
    const scene::Entity* spawn = nullptr;
    for (const auto& entity : scene.Entities()) {
        if (!entity.active) continue;
        if (entity.tag == "gameplay") {
            spawn = &entity;
            break;
        }
    }
    if (!spawn) {
        for (const auto& entity : scene.Entities()) {
            if (entity.active && entity.Has(scene::Component::Camera)) {
                spawn = &entity;
                break;
            }
        }
    }
    if (!spawn) return false;

    float yaw = player.yawDeg;
    if (spawn->Has(scene::Component::Camera) || true) {
        // Face whatever the actor's yaw says; Player Start authors often rotate it.
        yaw = 180.0f + spawn->transform.rotation.y;
    }
    player.SpawnAt(spawn->transform.position, yaw);
    if (spawn->Has(scene::Component::Camera)) {
        player.eyeHeight = std::max(0.6f, spawn->transform.position.y);
        player.position.y = spawn->transform.position.y;
    }
    return true;
}

}  // namespace astra::game
