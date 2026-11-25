#pragma once

#include "ParticleRuntime/ParticleSystem.h"

#include <array>
#include <filesystem>

namespace ParticleRuntimeDxLib
{
struct ViewportRect
{
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
};

struct OrbitCamera
{
    ParticleRuntime::Vector3 target{};
    float yaw = 0.55f;
    float pitch = 0.35f;
    float distance = 8.0f;
};

class ParticleRenderer
{
public:
    bool Initialize(const std::filesystem::path& particleImageDirectory);
    void Render(
        const ParticleRuntime::ParticleSystem& particleSystem,
        const OrbitCamera& camera,
        const ViewportRect& viewport);
    void Shutdown();

private:
    bool ResizeScene(int width, int height);

    std::array<int, 7> spriteHandles_{ -1, -1, -1, -1, -1, -1, -1 };
    int sceneHandle_ = -1;
    int sceneWidth_ = 0;
    int sceneHeight_ = 0;
};
}
