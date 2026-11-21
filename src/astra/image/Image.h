// image/Image.h - CPU image buffers the editor can generate, edit and export.
//
// Pixels are linear RGBA floats. The scripting host hands these out as integer handles, the
// script editor previews them as a thumbnail, and the PNG writer saves them to disk - so
// "image editing through scripts" is a real pipeline rather than an API stub.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::image {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
};

struct Buffer {
    int width = 0;
    int height = 0;
    std::vector<float> pixels;  // RGBA, row major

    Buffer() = default;
    Buffer(int w, int h) { Resize(w, h); }

    void Resize(int w, int h);
    bool Empty() const { return width <= 0 || height <= 0; }
    bool Contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    std::size_t Index(int x, int y) const {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) *
               4;
    }
    void Clear(const Color& color = Color(0.0f, 0.0f, 0.0f, 1.0f));
    void SetPixel(int x, int y, const Color& color);
    Color GetPixel(int x, int y) const;
    Color SampleBilinear(float u, float v) const;  // u,v in 0..1, clamped

    void FillRect(int x, int y, int w, int h, const Color& color);
    void FillCircle(int cx, int cy, float radius, const Color& color);
    void DrawLine(int x0, int y0, int x1, int y1, const Color& color);
    void GradientVertical(const Color& top, const Color& bottom);
    void Checkerboard(int cell, const Color& a, const Color& b);
    void AddNoise(float amount, std::uint32_t seed);
    void BoxBlur(int radius);
    void GammaCorrect(float gamma);
};

// Deterministic hash noise used by the image tools and the material shaders.
float HashNoise(int x, int y, std::uint32_t seed = 1337u);
float ValueNoise2D(float x, float y, std::uint32_t seed = 1337u);
float FractalNoise2D(float x, float y, int octaves = 4, std::uint32_t seed = 1337u);

bool WritePng(const Buffer& buffer, const std::string& path);
bool ReadPng(const std::string& path, Buffer& out);

}  // namespace astra::image
