#include "ParticleRuntime/ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr std::size_t MaxParticleCount = 10000;
}

namespace ParticleRuntime
{

void ParticleSystem::Play()
{
    playing_ = true;
}

void ParticleSystem::Pause()
{
    playing_ = false;
}

void ParticleSystem::Restart()
{
    ResetLoop();
    playing_ = true;
}

void ParticleSystem::StopAndClear()
{
    playing_ = false;
    ResetLoop();
}

void ParticleSystem::ResetLoop()
{
    particles_.clear();
    emissionAccumulator_ = 0.0f;
    elapsedTime_ = 0.0f;
    random_.seed(0xC0FFEEu);
}

void ParticleSystem::Update(float deltaTime)
{
    if (!playing_ || deltaTime <= 0.0f)
    {
        return;
    }

    deltaTime = std::min(deltaTime, 0.05f);
    float remainingTime = deltaTime;
    const float duration = std::max(settings_.duration, 0.1f);

    while (remainingTime > 0.0f)
    {
        const float timeUntilLoop = duration - elapsedTime_;
        if (timeUntilLoop <= 0.000001f)
        {
            ResetLoop();
            continue;
        }

        const float step = std::min(remainingTime, timeUntilLoop);
        Advance(step);
        elapsedTime_ += step;
        remainingTime -= step;

        if (elapsedTime_ >= duration - 0.000001f)
        {
            ResetLoop();
        }
    }
}

void ParticleSystem::Advance(float deltaTime)
{
    for (Particle& particle : particles_)
    {
        particle.age += deltaTime;
        particle.velocity.y += settings_.gravity * deltaTime;
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        particle.rotation += particle.rotationSpeed * deltaTime;
    }

    std::erase_if(particles_, [](const Particle& particle)
    {
        return particle.age >= particle.lifetime;
    });

    if (settings_.emissionRate <= 0.0f)
    {
        emissionAccumulator_ = 0.0f;
        return;
    }
    if (particles_.size() >= MaxParticleCount)
    {
        return;
    }

    const float interval = 1.0f / settings_.emissionRate;
    emissionAccumulator_ += deltaTime;
    while (emissionAccumulator_ >= interval && particles_.size() < MaxParticleCount)
    {
        emissionAccumulator_ -= interval;
        EmitOne();
    }
}

void ParticleSystem::EmitOne()
{
    std::uniform_real_distribution<float> spread(-settings_.spread, settings_.spread);
    std::uniform_real_distribution<float> positionOffset(-settings_.positionRandomness, settings_.positionRandomness);
    std::uniform_real_distribution<float> speedScale(1.0f - settings_.speedRandomness, 1.0f + settings_.speedRandomness);
    std::uniform_real_distribution<float> rotationOffset(-settings_.rotationRandomness, settings_.rotationRandomness);

    Vector3 direction = settings_.direction;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (directionLength > 0.0001f)
    {
        direction.x /= directionLength;
        direction.y /= directionLength;
        direction.z /= directionLength;
    }
    else
    {
        direction = { 0.0f, 1.0f, 0.0f };
    }

    Particle particle;
    particle.position = {
        settings_.position.x + positionOffset(random_),
        settings_.position.y + positionOffset(random_),
        settings_.position.z + positionOffset(random_)
    };
    const float initialSpeed = settings_.speed * speedScale(random_);
    particle.velocity = { direction.x * initialSpeed + spread(random_),
        direction.y * initialSpeed + spread(random_), direction.z * initialSpeed + spread(random_) };
    particle.lifetime = std::max(settings_.lifetime, 0.01f);
    particle.rotationSpeed = settings_.rotationSpeed + rotationOffset(random_);
    particles_.push_back(particle);
}

ParticleAppearance EvaluateAppearance(
    const Particle& particle,
    const ParticleEmitterSettings& settings)
{
    const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
    ParticleAppearance appearance;
    appearance.size = settings.startSize + (settings.endSize - settings.startSize) * t;
    appearance.rotation = particle.rotation;
    appearance.spriteIndex = settings.spriteIndex;
    for (std::size_t channel = 0; channel < appearance.color.size(); ++channel)
    {
        appearance.color[channel] = settings.startColor[channel] +
            (settings.endColor[channel] - settings.startColor[channel]) * t;
    }
    return appearance;
}
}
