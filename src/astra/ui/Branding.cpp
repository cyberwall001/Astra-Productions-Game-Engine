#include "astra/ui/Branding.h"

#include <map>

#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "imgui.h"
#include "stb/stb_image.h"

namespace astra::ui::branding {
namespace {

std::vector<Image> gImages;
std::map<std::string, std::size_t> gLookup;
std::function<void(Image&)> gUploader;
ImTextureID gNextTexture = 1000;

}  // namespace

void SetTextureUploader(std::function<void(Image&)> uploader) { gUploader = std::move(uploader); }

const Image* Load(const std::string& key, const std::string& path) {
    const auto existing = gLookup.find(key);
    if (existing != gLookup.end()) return &gImages[existing->second];

    std::vector<std::uint8_t> bytes;
    if (!fs::ReadBinaryFile(path, bytes)) {
        log::Warn("branding", "image not found: " + path);
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                 &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        log::Warn("branding", "could not decode image: " + path);
        if (pixels) stbi_image_free(pixels);
        return nullptr;
    }

    Image image;
    image.key = key;
    image.path = path;
    image.width = width;
    image.height = height;
    image.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(pixels);

    image.texture = gNextTexture++;
    if (gUploader) gUploader(image);

    gLookup[key] = gImages.size();
    gImages.push_back(std::move(image));
    log::Info("branding", "loaded " + key + " (" + std::to_string(width) + "x" +
                              std::to_string(height) + ")");
    return &gImages.back();
}

const Image* Logo() {
    static const Image* cached = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        cached = Load("logo", paths::Asset("branding/astra_logo.png"));
    }
    return cached;
}

const std::vector<Image>& Images() { return gImages; }

}  // namespace astra::ui::branding
