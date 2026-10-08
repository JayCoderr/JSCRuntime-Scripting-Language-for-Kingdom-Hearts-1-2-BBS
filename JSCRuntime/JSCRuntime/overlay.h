#pragma once

namespace Overlay
{
    bool Initialize();
    void Update();
    void Shutdown();
    void Resize(int width, int height);
    void Render();

    bool CreateRectangle(
        const char* imageSource,
        float x,
        float y,
        float width,
        float height,
        float alpha,
        int sort
    );
}