// ui/Branding.h - decoded image assets (logo, icons) shared by the GPU and the
// software preview renderer.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "imgui.h"

namespace astra::ui::branding {

struct Image {
    ImTextureID texture = 0;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;  // always RGBA8, top-left origin
    std::string key;
    std::string path;
};

// Called for every newly decoded image so a renderer backend can upload it.
// The software preview renderer leaves the ids as allocated here.
void SetTextureUploader(std::function<void(Image&)> uploader);

// Decodes `path` once and returns the cached image (nullptr on failure).
const Image* Load(const std::string& key, const std::string& path);

// The Astra Productions logo (assets/branding/astra_logo.png).
const Image* Logo();

// All decoded images.
const std::vector<Image>& Images();

}  // namespace astra::ui::branding
