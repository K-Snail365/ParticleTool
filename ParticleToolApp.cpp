#include "ParticleToolApp.h"

#include "ImGui/backends/imgui_impl_dx11.h"
#include "ImGui/backends/imgui_impl_win32.h"
#include "ImGui/imgui.h"
#include "ParticleRuntime/EffectFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
    LRESULT CALLBACK ImGuiHookProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
        return 0;
    }
    Vector3 Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Scale(const Vector3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
    float Dot(const Vector3& a, const Vector3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    Vector3 Cross(const Vector3& a, const Vector3& b) { return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x }; }
    Vector3 Normalize(const Vector3& v) { const float len=std::sqrt(Dot(v,v)); return len>0.0001f ? Scale(v,1.0f/len) : Vector3{}; }
}

bool ParticleToolApp::Initialize(HINSTANCE, int)
{
    SetGraphMode(1440, 900, 32);
    ChangeWindowMode(TRUE);
    SetUseDirect3DVersion(DX_DIRECT3D_11);
    if (DxLib_Init() == -1) return false;
    SetWindowTextW(GetMainWindowHandle(), L"ParticleTool");
    SetDrawScreen(DX_SCREEN_BACK);

    std::array<wchar_t, 32768> modulePath{};
    const DWORD pathLength=GetModuleFileNameW(nullptr,modulePath.data(),static_cast<DWORD>(modulePath.size()));
    if (pathLength==0 || pathLength>=modulePath.size()) { Shutdown(); return false; }
    const auto textureDirectory=std::filesystem::path(modulePath.data()).parent_path()/L"Assets"/L"Particles";
    if (!renderer_.Initialize(textureDirectory)) fileError_="Assets/Particles 内の PNG を読み込めませんでした。";

    auto* device=const_cast<ID3D11Device*>(static_cast<const ID3D11Device*>(GetUseDirect3D11Device()));
    auto* context=const_cast<ID3D11DeviceContext*>(static_cast<const ID3D11DeviceContext*>(GetUseDirect3D11DeviceContext()));
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
    if (!device || !context || !ImGui_ImplWin32_Init(GetMainWindowHandle()) ||
        !ImGui_ImplDX11_Init(device,context) || SetHookWinProc(ImGuiHookProc)==-1)
    {
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); Shutdown(); return false;
    }
    imguiInitialized_=true; UpdateWindowTitle(); previousFrameTime_=std::chrono::steady_clock::now(); return true;
}

int ParticleToolApp::Run()
{
    while (!exitRequested_ && ProcessMessage()==0 && CheckHitKey(KEY_INPUT_ESCAPE)==0)
    {
        const auto now=std::chrono::steady_clock::now();
        const float dt=std::chrono::duration<float>(now-previousFrameTime_).count(); previousFrameTime_=now;
        particleSystem_.Update(dt);
        SetDrawScreen(DX_SCREEN_BACK); SetDrawAreaFull(); ClearDrawScreen();
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawEditor(); ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); ScreenFlip();
    }
    Shutdown(); return 0;
}

void ParticleToolApp::DrawEditor()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Effect", "Ctrl+N")) NewEffect();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) OpenEffect();
            if (ImGui::MenuItem("Save", "Ctrl+S")) SaveEffect(false);
            if (ImGui::MenuItem("Save As...")) SaveEffect(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                exitRequested_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) NewEffect();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) OpenEffect();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveEffect(false);

    ImGui::SetNextWindowPos(ImVec2(12.0f, 36.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240.0f, 520.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Hierarchy"))
    {
        if (ImGui::TreeNodeEx("Particle Effect", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Selectable("Emitter", true);
            ImGui::TreePop();
        }
    }
    ImGui::End();

    ParticleEmitterSettings& settings = particleSystem_.Settings();
    ImGui::SetNextWindowPos(ImVec2(1030.0f, 36.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Inspector"))
    {
        constexpr const char* spriteNames[] = { "Blur circle", "Circle", "Heart", "Sparkle", "Square", "Star", "Triangle" };
        ImGui::TextUnformatted("Emitter");
        ImGui::Separator();
        ImGui::SliderFloat("Emission rate", &settings.emissionRate, 0.0f, 500.0f, "%.1f / sec");
        ImGui::SliderFloat("Lifetime", &settings.lifetime, 0.05f, 10.0f, "%.2f sec");
        ImGui::SliderFloat("Loop duration", &settings.duration, 0.1f, 60.0f, "%.2f sec");
        ImGui::Combo("Sprite", &settings.spriteIndex, spriteNames, IM_ARRAYSIZE(spriteNames));
        ImGui::SeparatorText("Spawn");
        ImGui::DragFloat3("Position", &settings.position.x, 0.05f);
        ImGui::SliderFloat("Position randomness", &settings.positionRandomness, 0.0f, 5.0f, "%.2f units");
        ImGui::DragFloat3("Direction", &settings.direction.x, 0.01f, -1.0f, 1.0f);
        ImGui::SliderFloat("Initial speed", &settings.speed, 0.0f, 10.0f, "%.2f units/sec");
        ImGui::SliderFloat("Speed randomness", &settings.speedRandomness, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Spread", &settings.spread, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Gravity", &settings.gravity, -10.0f, 10.0f, "%.2f units/sec^2");
        ImGui::SeparatorText("Appearance");
        ImGui::SliderFloat("Start size", &settings.startSize, 0.01f, 1.5f, "%.2f units");
        ImGui::SliderFloat("End size", &settings.endSize, 0.0f, 1.5f, "%.2f units");
        ImGui::SliderAngle("Rotation speed", &settings.rotationSpeed, -360.0f, 360.0f);
        ImGui::SliderAngle("Rotation randomness", &settings.rotationRandomness, 0.0f, 360.0f);
        ImGui::SeparatorText("Color over lifetime");
        ImGui::ColorEdit4("Start color", settings.startColor.data());
        ImGui::ColorEdit4("End color", settings.endColor.data());
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(260.0f, 36.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(750.0f, 590.0f), ImGuiCond_FirstUseEver);
    DrawViewport();

    ImGui::SetNextWindowPos(ImVec2(260.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(750.0f, 130.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Playback"))
    {
        if (ImGui::Button(particleSystem_.IsPlaying() ? "Pause" : "Play"))
        {
            if (particleSystem_.IsPlaying()) particleSystem_.Pause();
            else particleSystem_.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            particleSystem_.Restart();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            particleSystem_.StopAndClear();
        }
        ImGui::SameLine();
        ImGui::Text("%s | %.2f sec | %zu particles",
            particleSystem_.IsPlaying() ? "Playing" : "Paused",
            particleSystem_.ElapsedTime(),
            particleSystem_.Particles().size());
        ImGui::ProgressBar(
            settings.duration > 0.0f ? particleSystem_.ElapsedTime() / settings.duration : 0.0f,
            ImVec2(-1.0f, 0.0f));
    }
    ImGui::End();

    if (!fileError_.empty())
    {
        ImGui::OpenPopup("File Error");
    }
    if (ImGui::BeginPopupModal("File Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", fileError_.c_str());
        if (ImGui::Button("OK"))
        {
            fileError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ParticleToolApp::NewEffect()
{
    particleSystem_.StopAndClear(); particleSystem_.Settings()=ParticleEmitterSettings{};
    projectPath_.clear(); fileError_.clear(); UpdateWindowTitle();
}
void ParticleToolApp::OpenEffect()
{
    std::array<wchar_t,32768> filename{};
    const wchar_t filter[]=L"ParticleTool Effect (*.ptfx)\0*.ptfx\0All Files (*.*)\0*.*\0";
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=GetMainWindowHandle();
    dialog.lpstrFilter=filter; dialog.lpstrFile=filename.data(); dialog.nMaxFile=static_cast<DWORD>(filename.size());
    dialog.lpstrDefExt=L"ptfx"; dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) { if(CommDlgExtendedError()!=0) fileError_="Open ダイアログを開けませんでした。"; return; }
    ParticleEmitterSettings loaded; std::string error; const std::filesystem::path path(filename.data());
    if (!ParticleRuntime::LoadEffectFile(path,loaded,error)) { fileError_=error; return; }
    particleSystem_.StopAndClear(); particleSystem_.Settings()=loaded; projectPath_=path; fileError_.clear(); UpdateWindowTitle();
}
void ParticleToolApp::SaveEffect(bool saveAs)
{
    if(saveAs||projectPath_.empty())
    {
        std::array<wchar_t,32768> filename{};
        if(!projectPath_.empty()) { const auto existing=projectPath_.wstring(); wcsncpy_s(filename.data(),filename.size(),existing.c_str(),_TRUNCATE); }
        const wchar_t filter[]=L"ParticleTool Effect (*.ptfx)\0*.ptfx\0All Files (*.*)\0*.*\0";
        OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=GetMainWindowHandle();
        dialog.lpstrFilter=filter; dialog.lpstrFile=filename.data(); dialog.nMaxFile=static_cast<DWORD>(filename.size());
        dialog.lpstrDefExt=L"ptfx"; dialog.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;
        if(!GetSaveFileNameW(&dialog)) { if(CommDlgExtendedError()!=0) fileError_="Save ダイアログを開けませんでした。"; return; }
        projectPath_=filename.data();
    }
    std::string error; if(!ParticleRuntime::SaveEffectFile(projectPath_,particleSystem_.Settings(),error)) { fileError_=error; return; }
    fileError_.clear(); UpdateWindowTitle();
}
void ParticleToolApp::UpdateWindowTitle()
{
    std::wstring title=L"ParticleTool - "; title+=projectPath_.empty()?L"Untitled Effect":projectPath_.filename().wstring();
    SetWindowTextW(GetMainWindowHandle(),title.c_str());
}
void ParticleToolApp::DrawViewport()
{
    const auto flags=(cameraOrbitActive_||cameraPanActive_)?ImGuiWindowFlags_NoMove:ImGuiWindowFlags_None;
    if(!ImGui::Begin("Viewport",nullptr,flags)) { cameraOrbitActive_=cameraPanActive_=false; ImGui::End(); return; }
    const ImVec2 min=ImGui::GetCursorScreenPos(), size=ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##ViewportCameraInput",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered=ImGui::IsItemHovered();
    cameraOrbitActive_=ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left);
    cameraPanActive_=ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if(hovered)
    {
        const float wheel=ImGui::GetIO().MouseWheel; if(wheel!=0) cameraDistance_=std::clamp(cameraDistance_*std::pow(0.88f,wheel),2.0f,30.0f);
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) { cameraYaw_=0.55f; cameraPitch_=0.35f; cameraDistance_=8.0f; cameraTarget_={}; }
    }
    if(cameraOrbitActive_) { const auto d=ImGui::GetIO().MouseDelta; cameraYaw_+=d.x*0.008f; cameraPitch_=std::clamp(cameraPitch_+d.y*0.008f,-1.35f,1.35f); }
    const float cp=std::cos(cameraPitch_);
    const Vector3 offset{std::sin(cameraYaw_)*cp*cameraDistance_,std::sin(cameraPitch_)*cameraDistance_,std::cos(cameraYaw_)*cp*cameraDistance_};
    const Vector3 pos=Add(cameraTarget_,offset), forward=Normalize({cameraTarget_.x-pos.x,cameraTarget_.y-pos.y,cameraTarget_.z-pos.z});
    const Vector3 right=Normalize(Cross(forward,{0,1,0})), up=Normalize(Cross(right,forward));
    if(cameraPanActive_) { const auto d=ImGui::GetIO().MouseDelta; const float scale=cameraDistance_*0.002f; cameraTarget_=Add(cameraTarget_,Add(Scale(right,-d.x*scale),Scale(up,d.y*scale))); }
    renderer_.Render(particleSystem_,{cameraTarget_,cameraYaw_,cameraPitch_,cameraDistance_},
        {static_cast<int>(std::lround(min.x)),static_cast<int>(std::lround(min.y)),static_cast<int>(std::lround(std::max(size.x,1.0f))),static_cast<int>(std::lround(std::max(size.y,1.0f)))});
    ImGui::GetWindowDrawList()->AddText(ImVec2(min.x+12,min.y+10),IM_COL32(190,198,210,255),"LMB: orbit  |  RMB: pan  |  MMB: reset  |  Wheel: zoom");
    ImGui::End();
}
void ParticleToolApp::Shutdown()
{
    if(imguiInitialized_) { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); imguiInitialized_=false; }
    renderer_.Shutdown(); DxLib_End();
}
