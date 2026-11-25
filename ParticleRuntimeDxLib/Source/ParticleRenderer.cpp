#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ParticleRuntimeDxLib/ParticleRenderer.h"

#include <DxLib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
    constexpr std::array<const wchar_t*, 7> SpriteFileNames = {
        L"blur_circle.png", L"circle.png", L"heart.png", L"kirakira.png",
        L"square.png", L"star.png", L"triangle.png"
    };

    ParticleRuntime::Vector3 CameraPosition(const ParticleRuntimeDxLib::OrbitCamera& camera)
    {
        const float cosPitch = std::cos(camera.pitch);
        return {
            camera.target.x + std::sin(camera.yaw) * cosPitch * camera.distance,
            camera.target.y + std::sin(camera.pitch) * camera.distance,
            camera.target.z + std::cos(camera.yaw) * cosPitch * camera.distance
        };
    }

    float Depth(const ParticleRuntime::Vector3& point,
        const ParticleRuntime::Vector3& cameraPosition,
        const ParticleRuntime::Vector3& target)
    {
        const float dx = target.x - cameraPosition.x;
        const float dy = target.y - cameraPosition.y;
        const float dz = target.z - cameraPosition.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length <= 0.0001f) return 0.0f;
        return ((point.x - cameraPosition.x) * dx +
            (point.y - cameraPosition.y) * dy +
            (point.z - cameraPosition.z) * dz) / length;
    }

    int ToByte(float value)
    {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    }
}

bool ParticleRuntimeDxLib::ParticleRenderer::Initialize(
    const std::filesystem::path& particleImageDirectory)
{
    for (std::size_t index = 0; index < SpriteFileNames.size(); ++index)
    {
        const std::filesystem::path path = particleImageDirectory / SpriteFileNames[index];
        spriteHandles_[index] = LoadGraph(path.c_str());
        if (spriteHandles_[index] < 0)
        {
            Shutdown();
            return false;
        }
    }
    SetUseZBuffer3D(TRUE);
    SetWriteZBuffer3D(TRUE);
    return true;
}

bool ParticleRuntimeDxLib::ParticleRenderer::ResizeScene(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (sceneHandle_ >= 0 && sceneWidth_ == width && sceneHeight_ == height)
    {
        return true;
    }
    if (sceneHandle_ >= 0)
    {
        DeleteGraph(sceneHandle_);
        sceneHandle_ = -1;
    }
    sceneHandle_ = MakeScreen(width, height, FALSE);
    if (sceneHandle_ < 0)
    {
        sceneWidth_ = sceneHeight_ = 0;
        return false;
    }
    sceneWidth_ = width;
    sceneHeight_ = height;
    return true;
}

void ParticleRuntimeDxLib::ParticleRenderer::Render(
    const ParticleRuntime::ParticleSystem& particleSystem,
    const ParticleRuntimeDxLib::OrbitCamera& camera,
    const ParticleRuntimeDxLib::ViewportRect& viewport)
{
    if (viewport.width <= 0 || viewport.height <= 0 || !ResizeScene(viewport.width, viewport.height))
    {
        return;
    }

    SetDrawScreen(sceneHandle_);
    SetDrawAreaFull();
    ClearDrawScreen();
    DrawBox(0, 0, sceneWidth_, sceneHeight_, GetColor(18, 21, 27), TRUE);
    SetDrawArea(0, 0, sceneWidth_ - 1, sceneHeight_ - 1);

    SetupCamera_Perspective(DX_PI_F * 0.42f);
    SetCameraScreenCenter(sceneWidth_ * 0.5f, sceneHeight_ * 0.5f);
    SetCameraNearFar(0.05f, 500.0f);
    const ParticleRuntime::Vector3 cameraPosition = CameraPosition(camera);
    SetCameraPositionAndTarget_UpVecY(
        VGet(cameraPosition.x, cameraPosition.y, cameraPosition.z),
        VGet(camera.target.x, camera.target.y, camera.target.z));

    for (int grid = -10; grid <= 10; ++grid)
    {
        const unsigned int color = grid == 0 ? GetColor(65, 72, 83) : GetColor(39, 44, 53);
        DrawLine3D(VGet(static_cast<float>(grid), -0.01f, -10.0f), VGet(static_cast<float>(grid), -0.01f, 10.0f), color);
        DrawLine3D(VGet(-10.0f, -0.01f, static_cast<float>(grid)), VGet(10.0f, -0.01f, static_cast<float>(grid)), color);
    }
    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(2.0f, 0.0f, 0.0f), GetColor(235, 85, 78));
    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 2.0f, 0.0f), GetColor(105, 210, 125));
    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 0.0f, 2.0f), GetColor(90, 145, 245));

    struct RenderParticle
    {
        const ParticleRuntime::Particle* particle;
        ParticleRuntime::ParticleAppearance appearance;
        float depth;
    };
    std::vector<RenderParticle> particles;
    particles.reserve(particleSystem.Particles().size());
    for (const ParticleRuntime::Particle& particle : particleSystem.Particles())
    {
        particles.push_back({ &particle,
            ParticleRuntime::EvaluateAppearance(particle, particleSystem.Settings()),
            Depth(particle.position, cameraPosition, camera.target) });
    }
    std::sort(particles.begin(), particles.end(), [](const RenderParticle& a, const RenderParticle& b)
    {
        return a.depth > b.depth;
    });

    SetWriteZBuffer3D(FALSE);
    SetDrawMode(DX_DRAWMODE_BILINEAR);
    for (const RenderParticle& renderParticle : particles)
    {
        const ParticleRuntime::Particle& particle = *renderParticle.particle;
        const ParticleRuntime::ParticleAppearance& appearance = renderParticle.appearance;
        const int spriteIndex = std::clamp(appearance.spriteIndex, 0, static_cast<int>(spriteHandles_.size() - 1));
        if (spriteHandles_[spriteIndex] < 0 || appearance.color[3] <= 0.0f) continue;

        SetDrawBright(ToByte(appearance.color[0]), ToByte(appearance.color[1]), ToByte(appearance.color[2]));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, ToByte(appearance.color[3]));
        DrawBillboard3D(VGet(particle.position.x, particle.position.y, particle.position.z),
            0.5f, 0.5f, appearance.size, appearance.rotation,
            spriteHandles_[spriteIndex], TRUE);
    }
    SetDrawBright(255, 255, 255);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetWriteZBuffer3D(TRUE);
    SetDrawAreaFull();
    SetDrawScreen(DX_SCREEN_BACK);
    DrawExtendGraph(viewport.x, viewport.y, viewport.x + viewport.width, viewport.y + viewport.height,
        sceneHandle_, FALSE);
}

void ParticleRuntimeDxLib::ParticleRenderer::Shutdown()
{
    if (sceneHandle_ >= 0)
    {
        DeleteGraph(sceneHandle_);
        sceneHandle_ = -1;
    }
    sceneWidth_ = sceneHeight_ = 0;
    for (int& sprite : spriteHandles_)
    {
        if (sprite >= 0)
        {
            DeleteGraph(sprite);
            sprite = -1;
        }
    }
}
