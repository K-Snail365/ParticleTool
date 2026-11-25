#pragma once

#include <array>
#include <random>
#include <vector>

namespace ParticleRuntime
{
struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ParticleEmitterSettings
{
    float emissionRate = 32.0f;
    float lifetime = 1.8f;
    float duration = 5.0f;
    float speed = 2.0f;
    float speedRandomness = 0.25f;
    float spread = 0.55f;
    float gravity = -0.8f;
    Vector3 position{};
    Vector3 direction{ 0.0f, 1.0f, 0.0f };
    float positionRandomness = 0.0f;
    float rotationSpeed = 0.0f;
    float rotationRandomness = 0.0f;
    int spriteIndex = 0;
    float startSize = 0.18f;
    float endSize = 0.025f;
    std::array<float, 4> startColor{ 1.0f, 0.48f, 0.12f, 1.0f };
    std::array<float, 4> endColor{ 1.0f, 0.12f, 0.025f, 0.0f };
};

struct Particle
{
    Vector3 position;
    Vector3 velocity;
    float age = 0.0f;
    float lifetime = 1.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
};

struct ParticleAppearance
{
    float size = 0.0f;
    float rotation = 0.0f;
    int spriteIndex = 0;
    std::array<float, 4> color{};
};

class ParticleSystem
{
public:
    ParticleEmitterSettings& Settings() { return settings_; }
    const ParticleEmitterSettings& Settings() const { return settings_; }
    const std::vector<Particle>& Particles() const { return particles_; }

    bool IsPlaying() const { return playing_; }
    float ElapsedTime() const { return elapsedTime_; }

    void Play();
    void Pause();
    void Restart();
    void StopAndClear();
    void Update(float deltaTime);

private:
    void EmitOne();
    void Advance(float deltaTime);
    void ResetLoop();

    ParticleEmitterSettings settings_;
    std::vector<Particle> particles_;
    std::mt19937 random_{ 0xC0FFEEu };
    float emissionAccumulator_ = 0.0f;
    float elapsedTime_ = 0.0f;
    bool playing_ = false;
};

ParticleAppearance EvaluateAppearance(
    const Particle& particle,
    const ParticleEmitterSettings& settings);
}
