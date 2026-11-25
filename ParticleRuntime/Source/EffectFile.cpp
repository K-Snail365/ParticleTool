#include "ParticleRuntime/EffectFile.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace
{
    bool IsValid(const ParticleRuntime::ParticleEmitterSettings& settings)
    {
        const auto inRange = [](float value, float minimum, float maximum)
        {
            return std::isfinite(value) && value >= minimum && value <= maximum;
        };

        if (!inRange(settings.emissionRate, 0.0f, 500.0f) ||
            !inRange(settings.lifetime, 0.05f, 10.0f) ||
            !inRange(settings.duration, 0.1f, 60.0f) ||
            !inRange(settings.speed, 0.0f, 10.0f) ||
            !inRange(settings.speedRandomness, 0.0f, 1.0f) ||
            !inRange(settings.spread, 0.0f, 3.0f) ||
            !inRange(settings.gravity, -10.0f, 10.0f) ||
            !inRange(settings.positionRandomness, 0.0f, 5.0f) ||
            !inRange(settings.rotationSpeed, -6.28319f, 6.28319f) ||
            !inRange(settings.rotationRandomness, 0.0f, 6.28319f) ||
            !inRange(settings.startSize, 0.01f, 1.5f) ||
            !inRange(settings.endSize, 0.0f, 1.5f) ||
            settings.spriteIndex < 0 || settings.spriteIndex > 6)
        {
            return false;
        }

        for (float component : settings.startColor)
        {
            if (!inRange(component, 0.0f, 1.0f)) return false;
        }
        for (float component : settings.endColor)
        {
            if (!inRange(component, 0.0f, 1.0f)) return false;
        }
        for (float component : { settings.position.x, settings.position.y, settings.position.z,
            settings.direction.x, settings.direction.y, settings.direction.z })
        {
            if (!std::isfinite(component)) return false;
        }
        return true;
    }
}

namespace ParticleRuntime
{

bool SaveEffectFile(
    const std::filesystem::path& path,
    const ParticleEmitterSettings& settings,
    std::string& errorMessage)
{
    if (!IsValid(settings))
    {
        errorMessage = "Emitter settings contain a value outside the supported range.";
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        errorMessage = "Could not open the effect file for writing.";
        return false;
    }

    file << "ParticleToolEffect 3\n" << std::setprecision(9)
        << "emission_rate " << settings.emissionRate << '\n'
        << "lifetime " << settings.lifetime << '\n'
        << "duration " << settings.duration << '\n'
        << "speed " << settings.speed << '\n'
        << "speed_randomness " << settings.speedRandomness << '\n'
        << "spread " << settings.spread << '\n'
        << "gravity " << settings.gravity << '\n'
        << "position " << settings.position.x << ' ' << settings.position.y << ' ' << settings.position.z << '\n'
        << "direction " << settings.direction.x << ' ' << settings.direction.y << ' ' << settings.direction.z << '\n'
        << "position_randomness " << settings.positionRandomness << '\n'
        << "rotation_speed " << settings.rotationSpeed << '\n'
        << "rotation_randomness " << settings.rotationRandomness << '\n'
        << "sprite_index " << settings.spriteIndex << '\n'
        << "start_size " << settings.startSize << '\n'
        << "end_size " << settings.endSize << '\n'
        << "start_color " << settings.startColor[0] << ' ' << settings.startColor[1] << ' '
        << settings.startColor[2] << ' ' << settings.startColor[3] << '\n'
        << "end_color " << settings.endColor[0] << ' ' << settings.endColor[1] << ' '
        << settings.endColor[2] << ' ' << settings.endColor[3] << '\n';

    if (!file)
    {
        errorMessage = "An error occurred while writing the effect file.";
        return false;
    }
    errorMessage.clear();
    return true;
}

bool LoadEffectFile(
    const std::filesystem::path& path,
    ParticleEmitterSettings& settings,
    std::string& errorMessage)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        errorMessage = "Could not open the effect file.";
        return false;
    }

    std::string line;
    if (!std::getline(file, line))
    {
        errorMessage = "This is not a supported ParticleTool effect file.";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool versionOne = line == "ParticleToolEffect 1";
    const bool versionTwo = line == "ParticleToolEffect 2";
    const bool versionThree = line == "ParticleToolEffect 3";
    if (!versionOne && !versionTwo && !versionThree)
    {
        errorMessage = "This is not a supported ParticleTool effect file.";
        return false;
    }

    ParticleEmitterSettings loaded{};
    std::unordered_set<std::string> fields;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string key;
        if (!(row >> key) || !fields.insert(key).second)
        {
            errorMessage = "The effect file contains an invalid or duplicate field.";
            return false;
        }

        if (key == "start_color" || key == "end_color" || key == "position" || key == "direction")
        {
            if (key == "position" || key == "direction")
            {
                Vector3& vector = key == "position" ? loaded.position : loaded.direction;
                if (!(row >> vector.x >> vector.y >> vector.z))
                {
                    errorMessage = "The effect file contains an invalid vector value.";
                    return false;
                }
            }
            else
            {
                std::array<float, 4>& color = key == "start_color" ? loaded.startColor : loaded.endColor;
                if (!(row >> color[0] >> color[1] >> color[2] >> color[3]))
                {
                    errorMessage = "The effect file contains an invalid color value.";
                    return false;
                }
            }
        }
        else
        {
            float value = 0.0f;
            if (!(row >> value))
            {
                errorMessage = "The effect file contains an invalid numeric value.";
                return false;
            }
            if (key == "emission_rate") loaded.emissionRate = value;
            else if (key == "lifetime") loaded.lifetime = value;
            else if (key == "duration" && (versionTwo || versionThree)) loaded.duration = value;
            else if (key == "speed") loaded.speed = value;
            else if (key == "speed_randomness" && versionThree) loaded.speedRandomness = value;
            else if (key == "spread") loaded.spread = value;
            else if (key == "gravity") loaded.gravity = value;
            else if (key == "position_randomness" && versionThree) loaded.positionRandomness = value;
            else if (key == "rotation_speed" && versionThree) loaded.rotationSpeed = value;
            else if (key == "rotation_randomness" && versionThree) loaded.rotationRandomness = value;
            else if (key == "sprite_index" && versionThree) loaded.spriteIndex = static_cast<int>(value);
            else if (key == "start_size") loaded.startSize = value;
            else if (key == "end_size") loaded.endSize = value;
            else
            {
                errorMessage = "The effect file contains an unknown field.";
                return false;
            }
        }

        std::string extra;
        if (row >> extra)
        {
            errorMessage = "The effect file contains unexpected data.";
            return false;
        }
    }
    if (!file.eof())
    {
        errorMessage = "An error occurred while reading the effect file.";
        return false;
    }

    constexpr std::array<const char*, 9> requiredFields = {
        "emission_rate", "lifetime", "speed", "spread", "gravity",
        "start_size", "end_size", "start_color", "end_color"
    };
    for (const char* field : requiredFields)
    {
        if (!fields.contains(field))
        {
            errorMessage = "The effect file is missing a required field.";
            return false;
        }
    }
    if (versionTwo && !fields.contains("duration"))
    {
        errorMessage = "The effect file is missing the playback duration.";
        return false;
    }
    if (versionThree)
    {
        constexpr std::array<const char*, 8> versionThreeFields = {
            "speed_randomness", "position", "direction", "position_randomness",
            "rotation_speed", "rotation_randomness", "sprite_index", "duration"
        };
        for (const char* field : versionThreeFields)
        {
            if (!fields.contains(field))
            {
                errorMessage = "The effect file is missing a required field.";
                return false;
            }
        }
    }

    if (!IsValid(loaded))
    {
        errorMessage = "The effect file contains values outside the supported range.";
        return false;
    }

    settings = loaded;
    errorMessage.clear();
    return true;
}
}
