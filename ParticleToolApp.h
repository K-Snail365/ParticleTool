#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <DxLib.h>
#include <chrono>
#include <filesystem>
#include <string>

#include "ParticleRuntime/ParticleSystem.h"
#include "ParticleRuntimeDxLib/ParticleRenderer.h"

using ParticleRuntime::ParticleEmitterSettings;
using ParticleRuntime::ParticleSystem;
using ParticleRuntime::Vector3;

class ParticleToolApp
{
public:
    bool Initialize(HINSTANCE instance, int showCommand);
    int Run();

private:
    void DrawEditor();
    void DrawViewport();
    void NewEffect();
    void OpenEffect();
    void SaveEffect(bool saveAs);
    void UpdateWindowTitle();
    void Shutdown();

    ParticleRuntimeDxLib::ParticleRenderer renderer_;
    bool imguiInitialized_ = false;
    bool exitRequested_ = false;
    ParticleSystem particleSystem_;
    std::filesystem::path projectPath_;
    std::string fileError_;
    std::chrono::steady_clock::time_point previousFrameTime_{};
    float cameraYaw_ = 0.55f;
    float cameraPitch_ = 0.35f;
    float cameraDistance_ = 8.0f;
    Vector3 cameraTarget_{};
    bool cameraOrbitActive_ = false;
    bool cameraPanActive_ = false;
};
