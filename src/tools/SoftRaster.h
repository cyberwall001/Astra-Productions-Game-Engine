// tools/SoftRaster.h - a tiny CPU rasterizer for ImGui draw data.
//
// It exists so the studio UI can be rendered to PNG on a build machine with no GPU and
// no window, which is how the screenshots in docs/screenshots are produced and how the
// headless UI smoke test proves every screen draws without touching a renderer.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "imgui.h"

namespace soft {

struct Texture {
    const unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    bool alpha8 = true;  // font atlas, or 32-bit RGBA for images
};

using TextureResolver = std::function<bool(ImTextureID, Texture&)>;

struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;  // 4 bytes per pixel

    void Resize(int w, int h);
    void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
};

// Rasterizes every draw list in `data` into `fb` (which must already be sized/cleared).
// Returns the number of triangles drawn.
int RenderDrawData(ImDrawData* data, Framebuffer& fb, const TextureResolver& resolver);

// Saves the framebuffer as a 32-bit PNG (stored deflate blocks - no zlib dependency).
bool WritePng(const Framebuffer& fb, const std::string& path);

}  // namespace soft
