#include "tools/SoftRaster.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace soft {
namespace {

struct VertexOut {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void SampleTexture(const Texture& tex, float u, float v, float& outR, float& outG, float& outB,
                   float& outA) {
    if (!tex.pixels || tex.width <= 0 || tex.height <= 0) {
        outR = outG = outB = 1.0f;
        outA = 1.0f;
        return;
    }
    // Bilinear sample with clamped edges.
    const float x = Clamp01(u) * static_cast<float>(tex.width - 1);
    const float y = Clamp01(v) * static_cast<float>(tex.height - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, tex.width - 1);
    const int y1 = std::min(y0 + 1, tex.height - 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    auto fetch = [&](int px, int py, float& r, float& g, float& b, float& a) {
        if (tex.alpha8) {
            const unsigned char alpha = tex.pixels[static_cast<std::size_t>(py) * tex.width + px];
            r = g = b = 1.0f;
            a = static_cast<float>(alpha) / 255.0f;
        } else {
            const unsigned char* p =
                tex.pixels + (static_cast<std::size_t>(py) * tex.width + px) * 4;
            r = static_cast<float>(p[0]) / 255.0f;
            g = static_cast<float>(p[1]) / 255.0f;
            b = static_cast<float>(p[2]) / 255.0f;
            a = static_cast<float>(p[3]) / 255.0f;
        }
    };

    float r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
    fetch(x0, y0, r00, g00, b00, a00);
    fetch(x1, y0, r10, g10, b10, a10);
    fetch(x0, y1, r01, g01, b01, a01);
    fetch(x1, y1, r11, g11, b11, a11);

    auto mix = [&](float c00, float c10, float c01, float c11) {
        const float top = c00 + (c10 - c00) * fx;
        const float bottom = c01 + (c11 - c01) * fx;
        return top + (bottom - top) * fy;
    };
    outR = mix(r00, r10, r01, r11);
    outG = mix(g00, g10, g01, g11);
    outB = mix(b00, b10, b01, b11);
    outA = mix(a00, a10, a01, a11);
}

void BlendPixel(Framebuffer& fb, int x, int y, float r, float g, float b, float a) {
    if (a <= 0.0f) return;
    if (x < 0 || y < 0 || x >= fb.width || y >= fb.height) return;
    std::uint8_t* dst = fb.rgba.data() + (static_cast<std::size_t>(y) * fb.width + x) * 4;
    const float inv = 1.0f - a;
    auto toByte = [](float value) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    dst[0] = toByte(r * a + (static_cast<float>(dst[0]) / 255.0f) * inv);
    dst[1] = toByte(g * a + (static_cast<float>(dst[1]) / 255.0f) * inv);
    dst[2] = toByte(b * a + (static_cast<float>(dst[2]) / 255.0f) * inv);
    dst[3] = 255;
}

inline float EdgeFunction(const VertexOut& a, const VertexOut& b, float px, float py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

}  // namespace

void Framebuffer::Resize(int w, int h) {
    width = w;
    height = h;
    rgba.assign(static_cast<std::size_t>(w) * h * 4, 0);
}

void Framebuffer::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        rgba[i + 0] = r;
        rgba[i + 1] = g;
        rgba[i + 2] = b;
        rgba[i + 3] = a;
    }
}

int RenderDrawData(ImDrawData* data, Framebuffer& fb, const TextureResolver& resolver) {
    if (!data) return 0;
    int triangles = 0;

    for (int listIndex = 0; listIndex < data->CmdListsCount; ++listIndex) {
        const ImDrawList* list = data->CmdLists[listIndex];
        const ImDrawVert* vertices = list->VtxBuffer.Data;
        const ImDrawIdx* indices = list->IdxBuffer.Data;

        for (int cmdIndex = 0; cmdIndex < list->CmdBuffer.Size; ++cmdIndex) {
            const ImDrawCmd& cmd = list->CmdBuffer[cmdIndex];
            if (cmd.UserCallback != nullptr) continue;
            if (cmd.ElemCount == 0) continue;

            Texture texture;
            if (!resolver(cmd.GetTexID(), texture)) continue;

            const float clipMinX = std::max(cmd.ClipRect.x, 0.0f);
            const float clipMinY = std::max(cmd.ClipRect.y, 0.0f);
            const float clipMaxX = std::min(cmd.ClipRect.z, static_cast<float>(fb.width));
            const float clipMaxY = std::min(cmd.ClipRect.w, static_cast<float>(fb.height));
            if (clipMaxX <= clipMinX || clipMaxY <= clipMinY) continue;

            const ImDrawIdx* idx = indices + cmd.IdxOffset;
            for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3) {
                const ImDrawVert& v0 = vertices[idx[i + 0] + cmd.VtxOffset];
                const ImDrawVert& v1 = vertices[idx[i + 1] + cmd.VtxOffset];
                const ImDrawVert& v2 = vertices[idx[i + 2] + cmd.VtxOffset];

                VertexOut a, b, c;
                a.x = v0.pos.x; a.y = v0.pos.y; a.u = v0.uv.x; a.v = v0.uv.y;
                b.x = v1.pos.x; b.y = v1.pos.y; b.u = v1.uv.x; b.v = v1.uv.y;
                c.x = v2.pos.x; c.y = v2.pos.y; c.u = v2.uv.x; c.v = v2.uv.y;
                a.r = ((v0.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                a.g = ((v0.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                a.b = ((v0.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                a.a = ((v0.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                b.r = ((v1.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                b.g = ((v1.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                b.b = ((v1.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                b.a = ((v1.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                c.r = ((v2.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                c.g = ((v2.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                c.b = ((v2.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                c.a = ((v2.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;

                const int minX = static_cast<int>(
                    std::max(clipMinX, std::floor(std::min(a.x, std::min(b.x, c.x)))));
                const int maxX = static_cast<int>(
                    std::min(clipMaxX, std::ceil(std::max(a.x, std::max(b.x, c.x)))));
                const int minY = static_cast<int>(
                    std::max(clipMinY, std::floor(std::min(a.y, std::min(b.y, c.y)))));
                const int maxY = static_cast<int>(
                    std::min(clipMaxY, std::ceil(std::max(a.y, std::max(b.y, c.y)))));
                if (maxX <= minX || maxY <= minY) continue;

                float area = EdgeFunction(a, b, c.x, c.y);
                if (std::fabs(area) < 1e-6f) continue;
                const float invArea = 1.0f / area;
                ++triangles;

                for (int y = minY; y < maxY; ++y) {
                    const float py = static_cast<float>(y) + 0.5f;
                    for (int x = minX; x < maxX; ++x) {
                        const float px = static_cast<float>(x) + 0.5f;
                        float w0 = EdgeFunction(b, c, px, py) * invArea;
                        float w1 = EdgeFunction(c, a, px, py) * invArea;
                        float w2 = EdgeFunction(a, b, px, py) * invArea;
                        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                        const float u = a.u * w0 + b.u * w1 + c.u * w2;
                        const float v = a.v * w0 + b.v * w1 + c.v * w2;
                        float tr, tg, tb, ta;
                        SampleTexture(texture, u, v, tr, tg, tb, ta);

                        const float cr = a.r * w0 + b.r * w1 + c.r * w2;
                        const float cg = a.g * w0 + b.g * w1 + c.g * w2;
                        const float cb = a.b * w0 + b.b * w1 + c.b * w2;
                        const float ca = a.a * w0 + b.a * w1 + c.a * w2;

                        BlendPixel(fb, x, y, cr * tr, cg * tg, cb * tb, ca * ta);
                    }
                }
            }
        }
    }
    return triangles;
}

namespace {

std::uint32_t Crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu) {
    static std::uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }
    for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void WriteChunk(std::vector<std::uint8_t>& file, const char type[4],
                const std::vector<std::uint8_t>& payload) {
    WriteU32(file, static_cast<std::uint32_t>(payload.size()));
    const std::size_t crcStart = file.size();
    for (int i = 0; i < 4; ++i) file.push_back(static_cast<std::uint8_t>(type[i]));
    file.insert(file.end(), payload.begin(), payload.end());
    const std::uint32_t crc = Crc32(file.data() + crcStart, file.size() - crcStart) ^ 0xFFFFFFFFu;
    WriteU32(file, crc);
}

}  // namespace

bool WritePng(const Framebuffer& fb, const std::string& path) {
    if (fb.width <= 0 || fb.height <= 0) return false;

    // Raw scanlines: filter byte 0 + RGBA row.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(fb.height) * (1 + fb.width * 4));
    for (int y = 0; y < fb.height; ++y) {
        raw.push_back(0);
        const std::uint8_t* row = fb.rgba.data() + static_cast<std::size_t>(y) * fb.width * 4;
        raw.insert(raw.end(), row, row + static_cast<std::size_t>(fb.width) * 4);
    }

    // zlib stream using stored (uncompressed) deflate blocks.
    std::vector<std::uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    std::size_t offset = 0;
    while (offset < raw.size()) {
        const std::size_t blockSize = std::min<std::size_t>(65535, raw.size() - offset);
        const bool last = offset + blockSize >= raw.size();
        zlib.push_back(last ? 0x01 : 0x00);
        zlib.push_back(static_cast<std::uint8_t>(blockSize & 0xFF));
        zlib.push_back(static_cast<std::uint8_t>((blockSize >> 8) & 0xFF));
        const std::uint16_t nlen = static_cast<std::uint16_t>(~blockSize);
        zlib.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
        zlib.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                    raw.begin() + static_cast<std::ptrdiff_t>(offset + blockSize));
        offset += blockSize;
    }
    std::uint32_t s1 = 1;
    std::uint32_t s2 = 0;
    for (std::uint8_t byte : raw) {
        s1 = (s1 + byte) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    WriteU32(zlib, (s2 << 16) | s1);

    std::vector<std::uint8_t> file = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<std::uint8_t> ihdr;
    WriteU32(ihdr, static_cast<std::uint32_t>(fb.width));
    WriteU32(ihdr, static_cast<std::uint32_t>(fb.height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // RGBA
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    WriteChunk(file, "IHDR", ihdr);
    WriteChunk(file, "IDAT", zlib);
    WriteChunk(file, "IEND", {});

    FILE* handle = std::fopen(path.c_str(), "wb");
    if (!handle) return false;
    const bool ok = std::fwrite(file.data(), 1, file.size(), handle) == file.size();
    std::fclose(handle);
    return ok;
}

}  // namespace soft
