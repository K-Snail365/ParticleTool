#pragma once

#include "ParticleRuntime/ParticleSystem.h"

#include <filesystem>
#include <string>

namespace ParticleRuntime
{
bool SaveEffectFile(
    const std::filesystem::path& path,
    const ParticleEmitterSettings& settings,
    std::string& errorMessage);

bool LoadEffectFile(
    const std::filesystem::path& path,
    ParticleEmitterSettings& settings,
    std::string& errorMessage);
}
