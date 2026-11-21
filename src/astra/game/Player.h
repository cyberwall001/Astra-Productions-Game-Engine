// game/Player.h - the play-in-editor pawn.
//
// A deliberately small first-person controller: it owns the eye position and orientation,
// resolves the ground plane, and hands the viewport a camera. Keeping it free of ImGui means
// the play loop can be exercised by the headless tools (and later by a packaged build).
#pragma once

#include "astra/render/Camera.h"
#include "astra/scene/Scene.h"

namespace astra::game {

struct PlayerInput {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool sprint = false;
    float lookDeltaX = 0.0f;  // pixels
    float lookDeltaY = 0.0f;
};

struct Player {
    scene::Vec3 position{0.0f, 1.7f, 6.0f};
    float yawDeg = 180.0f;    // 180 faces -Z, the usual "into the level" default
    float pitchDeg = -4.0f;
    float velocityY = 0.0f;
    float eyeHeight = 1.7f;
    float walkSpeed = 4.6f;
    float sprintSpeed = 8.4f;
    float jumpSpeed = 5.2f;
    float gravity = 18.0f;
    float mouseSensitivity = 0.16f;
    bool onGround = true;

    void SpawnAt(const scene::Vec3& groundPosition, float yaw);
    void Reset();
    void Update(float dt, const PlayerInput& input);

    scene::Vec3 Eye() const;
    scene::Vec3 Forward() const;
    scene::Vec3 Right() const;
    // First-person view for the renderer: the camera sits at the eye.
    render::Camera ViewCamera(float fovDeg = 75.0f) const;
};

// Applies the level's Player Start (tagged "gameplay", falling back to the first actor with a
// camera) to a player. Returns false when the level has no spawn point.
bool SpawnFromLevel(Player& player, const scene::Scene& scene);

}  // namespace astra::game
