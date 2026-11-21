#include "astra/image/Image.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "astra/core/FileIO.h"
#include "stb/stb_image.h"

namespace astra::image {
namespace {

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

std::uint32_t Crc32(const std::uint8_t* data, std::size_t length) {
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void WriteChunk(std::vector<std::uint8_t>& file, const char* type,
                const std::vector<std::uint8_t>& payload) {
    WriteU32(file, static_cast<std::uint32_t>(payload.size()));
    const std::size_t start = file.size();
    file.insert(file.end(), type, type + 4);
    file.insert(file.end(), payload.begin(), payload.end());
    WriteU32(file, Crc32(file.data() + start, file.size() - start));
}

std::uint8_t ToByte(float value) {
    const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

}  // namespace

void Buffer::Resize(int w, int h) {
    width = std::max(0, w);
    height = std::max(0, h);
    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0.0f);
    for (std::size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 1.0f;
}

void Buffer::Clear(const Color& color) {
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        pixels[i] = color.r;
        pixels[i + 1] = color.g;
        pixels[i + 2] = color.b;
        pixels[i + 3] = color.a;
    }
}

void Buffer::SetPixel(int x, int y, const Color& color) {
    if (!Contains(x, y)) return;
    const std::size_t index = Index(x, y);
    pixels[index] = color.r;
    pixels[index + 1] = color.g;
    pixels[index + 2] = color.b;
    pixels[index + 3] = color.a;
}

Color Buffer::GetPixel(int x, int y) const {
    if (!Contains(x, y)) return Color(0.0f, 0.0f, 0.0f, 0.0f);
    const std::size_t index = Index(x, y);
    return Color(pixels[index], pixels[index + 1], pixels[index + 2], pixels[index + 3]);
}

Color Buffer::SampleBilinear(float u, float v) const {
    if (Empty()) return Color();
    const float fx = Clamp01(u) * static_cast<float>(width - 1);
    const float fy = Clamp01(v) * static_cast<float>(height - 1);
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const Color c00 = GetPixel(x0, y0);
    const Color c10 = GetPixel(x0 + 1, y0);
    const Color c01 = GetPixel(x0, y0 + 1);
    const Color c11 = GetPixel(x0 + 1, y0 + 1);
    const auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    Color out;
    out.r = mix(mix(c00.r, c10.r, tx), mix(c01.r, c11.r, tx), ty);
    out.g = mix(mix(c00.g, c10.g, tx), mix(c01.g, c11.g, tx), ty);
    out.b = mix(mix(c00.b, c10.b, tx), mix(c01.b, c11.b, tx), ty);
    out.a = mix(mix(c00.a, c10.a, tx), mix(c01.a, c11.a, tx), ty);
    return out;
}

void Buffer::FillRect(int x, int y, int w, int h, const Color& color) {
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) SetPixel(px, py, color);
    }
}

void Buffer::FillCircle(int cx, int cy, float radius, const Color& color) {
    const int minX = static_cast<int>(std::floor(cx - radius));
    const int maxX = static_cast<int>(std::ceil(cx + radius));
    const int minY = static_cast<int>(std::floor(cy - radius));
    const int maxY = static_cast<int>(std::ceil(cy + radius));
    const float r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float d2 = dx * dx + dy * dy;
            if (d2 > r2) continue;
            // One pixel of edge coverage so circles do not look serrated.
            const float edge = Clamp01((r2 - d2) / std::max(1.0f, radius * 2.0f));
            Color blended = color;
            blended.a *= Clamp01(0.25f + edge);
            const Color existing = GetPixel(x, y);
            const float a = blended.a;
            SetPixel(x, y,
                     Color(existing.r * (1 - a) + blended.r * a, existing.g * (1 - a) + blended.g * a,
                           existing.b * (1 - a) + blended.b * a,
                           std::max(existing.a, blended.a)));
        }
    }
}

void Buffer::DrawLine(int x0, int y0, int x1, int y1, const Color& color) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        SetPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) {
            if (x0 == x1) break;
            error += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            if (y0 == y1) break;
            error += dx;
            y0 += sy;
        }
    }
}

void Buffer::GradientVertical(const Color& top, const Color& bottom) {
    for (int y = 0; y < height; ++y) {
        const float t = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
        const Color color(top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t,
                          top.b + (bottom.b - top.b) * t, top.a + (bottom.a - top.a) * t);
        for (int x = 0; x < width; ++x) SetPixel(x, y, color);
    }
}

void Buffer::Checkerboard(int cell, const Color& a, const Color& b) {
    const int size = std::max(1, cell);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool odd = ((x / size) + (y / size)) % 2 != 0;
            SetPixel(x, y, odd ? b : a);
        }
    }
}

void Buffer::AddNoise(float amount, std::uint32_t seed) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float n = (HashNoise(x, y, seed) - 0.5f) * amount;
            const std::size_t index = Index(x, y);
            pixels[index] = Clamp01(pixels[index] + n);
            pixels[index + 1] = Clamp01(pixels[index + 1] + n);
            pixels[index + 2] = Clamp01(pixels[index + 2] + n);
        }
    }
}

void Buffer::BoxBlur(int radius) {
    if (radius <= 0 || Empty()) return;
    std::vector<float> source = pixels;
    const int r = radius;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum[4] = {0, 0, 0, 0};
            int count = 0;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    const int sx = std::clamp(x + dx, 0, width - 1);
                    const int sy = std::clamp(y + dy, 0, height - 1);
                    const std::size_t index =
                        (static_cast<std::size_t>(sy) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(sx)) *
                        4;
                    sum[0] += source[index];
                    sum[1] += source[index + 1];
                    sum[2] += source[index + 2];
                    sum[3] += source[index + 3];
                    ++count;
                }
            }
            const std::size_t index = Index(x, y);
            for (int c = 0; c < 4; ++c) {
                pixels[index + c] = sum[c] / static_cast<float>(std::max(1, count));
            }
        }
    }
}

void Buffer::GammaCorrect(float gamma) {
    if (gamma <= 0.0f) return;
    for (float& value : pixels) value = std::pow(Clamp01(value), 1.0f / gamma);
}

float HashNoise(int x, int y, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFF) / 16777215.0f;
}

float ValueNoise2D(float x, float y, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float n00 = HashNoise(x0, y0, seed);
    const float n10 = HashNoise(x0 + 1, y0, seed);
    const float n01 = HashNoise(x0, y0 + 1, seed);
    const float n11 = HashNoise(x0 + 1, y0 + 1, seed);
    const float top = n00 + (n10 - n00) * sx;
    const float bottom = n01 + (n11 - n01) * sx;
    return top + (bottom - top) * sy;
}

float FractalNoise2D(float x, float y, int octaves, std::uint32_t seed) {
    float sum = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float normalisation = 0.0f;
    for (int i = 0; i < std::max(1, octaves); ++i) {
        sum += ValueNoise2D(x * frequency, y * frequency, seed + static_cast<std::uint32_t>(i) * 17u) *
               amplitude;
        normalisation += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return normalisation > 0.0f ? sum / normalisation : 0.0f;
}

bool WritePng(const Buffer& buffer, const std::string& path) {
    if (buffer.Empty()) return false;

    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(buffer.height) * (1 + static_cast<std::size_t>(buffer.width) * 4));
    for (int y = 0; y < buffer.height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < buffer.width; ++x) {
            const std::size_t index = buffer.Index(x, y);
            raw.push_back(ToByte(buffer.pixels[index]));
            raw.push_back(ToByte(buffer.pixels[index + 1]));
            raw.push_back(ToByte(buffer.pixels[index + 2]));
            raw.push_back(ToByte(buffer.pixels[index + 3]));
        }
    }

    // Stored (uncompressed) deflate blocks keep the encoder dependency-free; the editor
    // recompresses generated screenshots separately anyway.
    std::vector<std::uint8_t> zlib = {0x78, 0x01};
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
    WriteU32(ihdr, static_cast<std::uint32_t>(buffer.width));
    WriteU32(ihdr, static_cast<std::uint32_t>(buffer.height));
    ihdr.push_back(8);
    ihdr.push_back(6);
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

bool ReadPng(const std::string& path, Buffer& out) {
    std::vector<std::uint8_t> bytes;
    if (!fs::ReadBinaryFile(path, bytes) || bytes.empty()) return false;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                               &width, &height, &channels, 4);
    if (!data) return false;
    out.Resize(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t source =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)) *
                4;
            const std::size_t target = out.Index(x, y);
            for (int c = 0; c < 4; ++c) {
                out.pixels[target + c] = static_cast<float>(data[source + c]) / 255.0f;
            }
        }
    }
    stbi_image_free(data);
    return true;
}

}  // namespace astra::image
