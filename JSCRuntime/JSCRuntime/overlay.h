
#pragma once

namespace Overlay
{
    extern bool g_scriptKeyDown[256];
    extern bool g_scriptKeyPressed[256];
    extern bool scriptLoaded;
    extern bool scriptFile;

    bool Initialize();
    void Update();
    void Shutdown();
    void Resize(int width, int height);
    void Render();
}
