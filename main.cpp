#include "ParticleToolApp.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    ParticleToolApp app;
    if (!app.Initialize(instance, showCommand))
    {
        return 1;
    }

    return app.Run();
}
