// app/screens/EditorWindows.cpp - floating editor windows (Render / ray tracing, World Settings).
//
// Both are ordinary ImGui windows so they can be dragged anywhere, which is also the fix for
// the old "panels cannot be moved" complaint: the dock-style panels stay fixed, and anything
// that needs to float lives in a real window with a title bar.
#include <algorithm>
#include <cstdio>
#include <string>

#include "astra/app/Screens.h"
#include "astra/asset/Bridge.h"
#include "astra/app/screens/Common.h"
#include "astra/app/screens/EditorPanels.h"
#include "astra/core/Log.h"
#include "astra/core/Strings.h"
#include "astra/render/Pathtracer.h"
#include "astra/ui/Theme.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace astra::app::panels {
namespace {

using ui::Colors;
using ui::Text;
using ui::U;

const char* const kBackends[] = {"CPU (rasterised path tracer)", "GPU (unavailable here)"};

// Draws an image::Buffer into the draw list as a grid of small quads. No texture upload, so the
// panel looks identical in the GPU studio and the software preview used by the headless tests.
void DrawBufferPreview(const image::Buffer& buffer, ImVec2 pos, ImVec2 size) {
    if (buffer.Empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().surfaceSunken);

    // Aspect-fit inside the box.
    const float aspect = static_cast<float>(buffer.width) / static_cast<float>(buffer.height);
    float drawWidth = size.x;
    float drawHeight = size.x / aspect;
    if (drawHeight > size.y) {
        drawHeight = size.y;
        drawWidth = size.y * aspect;
    }
    const ImVec2 imageMin(pos.x + (size.x - drawWidth) * 0.5f, pos.y + (size.y - drawHeight) * 0.5f);

    // One quad per screen block, sampling the render with bilinear filtering. The block count is
    // capped because every quad costs four draw-list vertices and ImGui indexes with 16 bits.
    const int cellsX = std::clamp(buffer.width, 16, 96);
    const int cellsY = std::clamp(static_cast<int>(cellsX / aspect), 8, 54);
    const float cellW = drawWidth / static_cast<float>(cellsX);
    const float cellH = drawHeight / static_cast<float>(cellsY);
    for (int y = 0; y < cellsY; ++y) {
        for (int x = 0; x < cellsX; ++x) {
            const image::Color c = buffer.SampleBilinear((static_cast<float>(x) + 0.5f) / cellsX,
                                                         (static_cast<float>(y) + 0.5f) / cellsY);
            const auto toByte = [](float value) {
                return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            const ImU32 packed = IM_COL32(toByte(c.r), toByte(c.g), toByte(c.b), 255);
            const ImVec2 cellMin(imageMin.x + cellW * x, imageMin.y + cellH * y);
            const ImVec2 cellMax(cellMin.x + cellW + 1.0f, cellMin.y + cellH + 1.0f);
            dl->AddRectFilled(cellMin, cellMax, packed);
        }
    }
    dl->AddRect(imageMin, ImVec2(imageMin.x + drawWidth, imageMin.y + drawHeight), Colors().stroke,
                U(3.0f));
}

// Every row is positioned explicitly: the hand-drawn widgets are laid out in screen space, so
// SameLine() is not reliable between them.
void LabeledSlider(const char* label, int* value, int minValue, int maxValue, const char* format,
                   float y) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float x = window->DC.CursorStartPos.x;
    window->DrawList->AddText(Text().small, Text().small->FontSize, ImVec2(x, y + U(9.0f)),
                              Colors().textDim, label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(6.0f));
    ImGui::SetCursorScreenPos(ImVec2(x + U(150.0f), y + U(3.0f)));
    ImGui::SetNextItemWidth(U(104.0f));
    ImGui::DragInt(("##" + std::string(label)).c_str(), value, 1.0f, minValue, maxValue, format);
    ImGui::PopStyleVar();
}

void LabeledFloat(const char* label, float* value, float speed, float minValue, float maxValue,
                  const char* format, float y) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float x = window->DC.CursorStartPos.x;
    window->DrawList->AddText(Text().small, Text().small->FontSize, ImVec2(x, y + U(9.0f)),
                              Colors().textDim, label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(6.0f));
    ImGui::SetCursorScreenPos(ImVec2(x + U(150.0f), y + U(3.0f)));
    ImGui::SetNextItemWidth(U(104.0f));
    ImGui::DragFloat(("##" + std::string(label)).c_str(), value, speed, minValue, maxValue, format);
    ImGui::PopStyleVar();
}

// A checkbox whose state is mirrored in a local bool so the row can be positioned freely.
void CheckboxAt(const char* label, bool* value, ImVec2 position) {
    ImGui::SetCursorScreenPos(position);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
    ImGui::Checkbox(label, value);
    ImGui::PopStyleColor();
}

}  // namespace

void RenderPanel(AstraApp& app) {
    if (!app.editor.showRenderPanel) return;
    auto& traceState = app.editor.trace;

    ImGui::SetNextWindowSize(ImVec2(U(486.0f), U(648.0f)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.5f - U(235.0f), U(96.0f)),
                            ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("Render - Path Tracer###render_panel", &open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) app.editor.showRenderPanel = false;
        return;
    }
    if (!open) app.editor.showRenderPanel = false;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    ImGui::Dummy(ImVec2(0.0f, 0.0f));  // settle the cursor at the content origin
    const float left = window->DC.CursorStartPos.x;
    const float top = window->DC.CursorStartPos.y;
    const float rowHeight = U(27.0f);
    const float fieldX = left + U(150.0f);

    float y = top;
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textDim,
                "OFFLINE RENDER");
    y += U(18.0f);
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().textDim,
                "CPU tracer: BVH, soft sun shadows, ambient occlusion, reflections.");
    y += U(26.0f);
    dl->AddLine(ImVec2(left, y), ImVec2(left + ImGui::GetContentRegionAvail().x, y),
                Colors().stroke, U(1.0f));
    y += U(12.0f);

    LabeledSlider("Width", &traceState.width, 32, 1920, "%d px", y);
    y += rowHeight;
    LabeledSlider("Height", &traceState.height, 32, 1080, "%d px", y);
    y += rowHeight;
    LabeledSlider("Samples", &traceState.samples, 1, 512, "%d spp", y);
    y += rowHeight;
    LabeledSlider("Bounces", &traceState.maxBounces, 1, 12, "%d", y);
    y += rowHeight;
    LabeledFloat("Exposure", &traceState.exposure, 0.02f, 0.05f, 8.0f, "%.2f", y);
    y += rowHeight;
    LabeledFloat("Sun size", &traceState.sunAngularRadiusDeg, 0.05f, 0.0f, 12.0f, "%.2f deg", y);
    y += rowHeight + U(4.0f);

    CheckboxAt("Ambient occlusion", &traceState.ambientOcclusion, ImVec2(left, y));
    CheckboxAt("Reflections", &traceState.reflections, ImVec2(left + U(150.0f), y));
    CheckboxAt("Denoise", &traceState.denoise, ImVec2(left + U(276.0f), y));
    y += U(28.0f);

    // Backend picker: GPU is honestly reported as unavailable on this build.
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y + U(8.0f)), Colors().textDim,
                "Backend");
    const int backend = traceState.backend == render::TraceBackend::Gpu ? 1 : 0;
    ImGui::SetCursorScreenPos(ImVec2(fieldX, y + U(2.0f)));
    ImGui::SetNextItemWidth(U(220.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(6.0f));
    if (ImGui::BeginCombo("##trace_backend", render::TraceBackendName(traceState.backend))) {
        if (ImGui::Selectable("CPU (multi-threaded path tracer)", backend == 0)) {
            traceState.backend = render::TraceBackend::Cpu;
        }
        const bool gpuOk = render::TraceBackendAvailable(render::TraceBackend::Gpu);
        if (ImGui::Selectable("GPU (not available in this build)", backend == 1, 0,
                              gpuOk ? ImVec2(0, 0) : ImVec2(0, 0))) {
            if (gpuOk) {
                traceState.backend = render::TraceBackend::Gpu;
            } else {
                app.PushToast(ToastKind::Warning, "GPU tracer unavailable",
                              "This build has no compute backend, so rendering stays on the CPU.");
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    y += U(40.0f);

    const auto trace = [&]() {
        if (app.editor.tracing) return;
        app.StartTrace();
        if (!app.editor.tracing) return;
        render::TraceScene baked;
        baked.Build(app.editor.scene);
        const render::Camera camera = app.editor.playing ? app.editor.player.ViewCamera()
                                                        : app.editor.viewportCamera;
        app.editor.traceImage =
            render::Render(baked, camera, traceState, &app.editor.traceStats);
        app.editor.traceProgress = 1.0f;
        app.editor.tracing = false;
        app.editor.traceError.clear();
        if (app.editor.traceImage.Empty()) {
            app.editor.traceError = "Nothing to render - the level has no visible geometry.";
        } else {
            app.SetFooterStatus(str::Format("Rendered %dx%d in %.0f ms", traceState.width,
                                            traceState.height,
                                            app.editor.traceStats.milliseconds));
            log::Info("render", str::Format("trace %dx%d %d spp -> %.0f ms, %lld rays",
                                            traceState.width, traceState.height, traceState.samples,
                                            app.editor.traceStats.milliseconds,
                                            static_cast<long long>(app.editor.traceStats.rays)));
        }
    };

    ImGui::SetCursorScreenPos(ImVec2(left, y));
    if (ui::PrimaryButton("Render frame", ImVec2(U(132.0f), U(32.0f)), !app.editor.tracing,
                          ui::Icon::Play)) {
        trace();
    }
    ImGui::SetCursorScreenPos(ImVec2(left + U(142.0f), y));
    if (ui::SecondaryButton("Save PNG", ImVec2(U(104.0f), U(32.0f)),
                            !app.editor.traceImage.Empty(), ui::Icon::Download)) {
        app.SaveTraceImage();
    }
    ImGui::SetCursorScreenPos(ImVec2(left + U(256.0f), y));
    if (ui::GhostButton("Close", ImVec2(U(76.0f), U(32.0f)))) app.editor.showRenderPanel = false;
    y += U(40.0f);

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    char stats[192];
    std::snprintf(stats, sizeof(stats), "%d tris  -  %d BVH nodes  -  %d threads  -  %lld rays  -  %.0f ms",
                  app.editor.traceStats.triangles, app.editor.traceStats.nodes,
                  app.editor.traceStats.threads, static_cast<long long>(app.editor.traceStats.rays),
                  app.editor.traceStats.milliseconds);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textFaint, stats);
    y += U(18.0f);
    if (!app.editor.traceError.empty()) {
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().danger,
                    app.editor.traceError.c_str());
        y += U(18.0f);
    }

    const ImVec2 previewPos(left, y);
    // Stay inside the window: the preview fills whatever is left after the controls.
    const float bottomEdge = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y -
                             ImGui::GetStyle().WindowPadding.y;
    const ImVec2 previewSize(contentWidth, std::max(U(88.0f), bottomEdge - y - U(6.0f)));
    ImGui::SetCursorScreenPos(previewPos);
    ImGui::InvisibleButton("##trace_preview", previewSize);
    if (!app.editor.traceImage.Empty()) {
        DrawBufferPreview(app.editor.traceImage, previewPos, previewSize);
    } else {
        dl->AddRectFilled(previewPos, ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
                          Colors().surfaceSunken, U(6.0f));
        const char* hint = "Render a frame to see it here";
        const float width = Text().small->CalcTextSizeA(Text().small->FontSize, FLT_MAX, 0.0f,
                                                        hint).x;
        dl->AddText(Text().small, Text().small->FontSize,
                    ImVec2(previewPos.x + (previewSize.x - width) * 0.5f,
                           previewPos.y + previewSize.y * 0.5f - U(8.0f)),
                    Colors().textFaint, hint);
    }

    ImGui::End();
}

// Draws a shaded swatch for a material: the same procedural patterns the viewport uses,
// reduced to a 2D preview so the editor can show the result without a render target.
void DrawMaterialPreview(ImDrawList* dl, ImVec2 centre, float radius,
                         const asset::MaterialAsset& material) {
    const auto toColor = [](const scene::Vec3& color) {
        const float alpha = std::clamp(color.x * 0.0f + 1.0f, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(
            ImVec4(std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f),
                   std::clamp(color.z, 0.0f, 1.0f), alpha));
    };
    const ImU32 base = toColor(material.baseColor);
    const ImU32 accent = toColor(material.accentColor);
    dl->AddCircleFilled(centre, radius, ui::Fade(base, 0.55f), 48);

    const ImVec2 min(centre.x - radius, centre.y - radius);
    const ImVec2 max(centre.x + radius, centre.y + radius);
    const int shader = static_cast<int>(material.shader);
    // Only the clip circle is filled, so every pattern stays inside the sphere.
    dl->PushClipRect(min, max, true);
    if (shader == static_cast<int>(scene::MaterialShader::Checker)) {
        const float cell = ImMax(4.0f, radius / 4.0f);
        for (float y = min.y; y < max.y; y += cell) {
            for (float x = min.x; x < max.x; x += cell) {
                const int checker = (static_cast<int>((x - min.x) / cell) +
                                     static_cast<int>((y - min.y) / cell)) & 1;
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell, y + cell),
                                  checker ? base : accent);
            }
        }
    } else if (shader == static_cast<int>(scene::MaterialShader::Stripes)) {
        const float band = ImMax(3.0f, radius / 6.0f);
        for (float y = min.y; y < max.y; y += band * 2.0f) {
            dl->AddRectFilled(ImVec2(min.x, y), ImVec2(max.x, y + band), accent);
        }
        dl->AddCircleFilled(centre, radius * 0.55f, base, 40);
    } else if (shader == static_cast<int>(scene::MaterialShader::Noise) ||
               shader == static_cast<int>(scene::MaterialShader::Gradient)) {
        dl->AddRectFilledMultiColor(min, max, accent, base, base, accent);
        dl->AddCircleFilled(centre, radius * 0.62f, ui::Fade(base, 0.55f), 40);
    } else {
        dl->AddCircleFilled(centre, radius, base, 48);
        if (shader == static_cast<int>(scene::MaterialShader::Hologram)) {
            for (float y = min.y; y < max.y; y += ImMax(3.0f, radius / 7.0f)) {
                dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), ui::Fade(accent, 0.65f),
                            ImMax(1.0f, U(1.0f)));
            }
        }
    }
    dl->PopClipRect();

    // A soft top-left highlight sells the "lit sphere" read.
    dl->AddCircleFilled(ImVec2(centre.x - radius * 0.32f, centre.y - radius * 0.34f),
                        radius * 0.34f, ui::Fade(IM_COL32(255, 255, 255, 255), 0.16f), 32);
    dl->AddCircle(centre, radius, ui::Fade(IM_COL32(255, 255, 255, 255), 0.22f), 48,
                  ImMax(1.0f, U(1.0f)));
}

void MaterialEditor(AstraApp& app) {
    if (!app.editor.showMaterialEditor) return;

    // A real ImGui window so it can be dragged like any other tool window. The height is
    // fixed: the columns are laid out by hand, so shrinking would clip fields instead of
    // reflowing them.
    ImGui::SetNextWindowSize(ImVec2(U(700.0f), U(452.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.5f - U(350.0f), U(96.0f)),
                            ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors().editorPanel);
    bool open = true;
    if (!ImGui::Begin("Material Editor", &open)) {
        ImGui::End();
        ImGui::PopStyleColor();
        if (!open) app.editor.showMaterialEditor = false;
        return;
    }
    if (!open) app.editor.showMaterialEditor = false;

    asset::MaterialLibrary& library = app.editor.materials;
    const auto& materials = library.All();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const float sizeX = ImGui::GetContentRegionAvail().x;
    const float left = window->DC.CursorStartPos.x;
    float y = window->DC.CursorStartPos.y;
    const float listWidth = U(176.0f);
    const float fieldX = left + listWidth + U(22.0f);
    const float fieldWidth = ImMax(U(180.0f), sizeX - listWidth - U(24.0f));
    const float rowHeight = U(27.0f);

    if (materials.empty()) {
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().textDim,
                    "No materials yet - open a project to seed <project>/materials.");
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    app.editor.materialIndex =
        std::clamp(app.editor.materialIndex, 0, static_cast<int>(materials.size()) - 1);
    asset::MaterialAsset& material =
        const_cast<asset::MaterialAsset&>(materials[static_cast<std::size_t>(app.editor.materialIndex)]);

    // ---- library list on a sunken card ------------------------------------
    const float listHeight = U(206.0f);
    dl->AddRectFilled(ImVec2(left - U(6.0f), y - U(6.0f)),
                      ImVec2(left + listWidth + U(6.0f), y + listHeight + U(6.0f)),
                      Colors().surfaceSunken, U(8.0f));
    dl->AddRect(ImVec2(left - U(6.0f), y - U(6.0f)),
                ImVec2(left + listWidth + U(6.0f), y + listHeight + U(6.0f)), Colors().stroke,
                U(8.0f), 0, U(1.0f));
    ImGui::SetCursorScreenPos(ImVec2(left, y));
    ImGui::PushStyleColor(ImGuiCol_Header, ui::ToVec4(ui::Fade(Colors().brand, 0.28f)));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui::ToVec4(Colors().surfaceHover));
    ImGui::BeginChild("##material_list", ImVec2(listWidth, listHeight), ImGuiChildFlags_None);
    for (int i = 0; i < static_cast<int>(materials.size()); ++i) {
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
        if (ImGui::Selectable(materials[i].name.c_str(), i == app.editor.materialIndex,
                              ImGuiSelectableFlags_None, ImVec2(0.0f, U(24.0f)))) {
            app.editor.materialIndex = i;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    DrawMaterialPreview(dl, ImVec2(left + listWidth * 0.5f, y + listHeight + U(92.0f)), U(58.0f),
                        material);
    const char* shaderLabel = scene::MaterialShaderName(material.shader);
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(left + listWidth * 0.5f -
                           Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f,
                                                      shaderLabel)
                                   .x *
                               0.5f,
                       y + listHeight + U(156.0f)),
                Colors().textFaint, shaderLabel);

    // ---- fields ------------------------------------------------------------
    float fieldY = y;
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(fieldX, fieldY), Colors().textFaint,
                "MATERIAL");
    fieldY += U(20.0f);

    char nameBuffer[96];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", material.name.c_str());
    ImGui::SetCursorScreenPos(ImVec2(fieldX, fieldY));
    ImGui::SetNextItemWidth(fieldWidth);
    if (ImGui::InputText("##material_name", nameBuffer, sizeof(nameBuffer))) {
        material.name = nameBuffer;
    }
    fieldY += rowHeight + U(8.0f);

    auto label = [&](const char* text, float rowY) {
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(fieldX, rowY + U(5.0f)),
                    Colors().textDim, text);
    };

    label("Shader", fieldY);
    ImGui::SetCursorScreenPos(ImVec2(fieldX + U(96.0f), fieldY));
    ImGui::SetNextItemWidth(fieldWidth - U(96.0f));
    int shaderIndex = static_cast<int>(material.shader);
    int shaderCount = 0;
    const char* const* shaderNames = scene::MaterialShaderNames(shaderCount);
    if (ImGui::Combo("##material_shader", &shaderIndex, shaderNames, shaderCount)) {
        material.shader = static_cast<scene::MaterialShader>(shaderIndex);
    }
    fieldY += rowHeight + U(6.0f);

    auto colorRow = [&](const char* text, const char* id, scene::Vec3& color) {
        label(text, fieldY);
        ImGui::SetCursorScreenPos(ImVec2(fieldX + U(96.0f), fieldY));
        ImGui::SetNextItemWidth(fieldWidth - U(96.0f));
        ImGui::ColorEdit3(id, &color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        fieldY += rowHeight + U(2.0f);
    };
    colorRow("Base", "##material_base", material.baseColor);
    colorRow("Accent", "##material_accent", material.accentColor);

    fieldY += U(4.0f);
    auto sliderRow = [&](const char* text, const char* id, float& value, float minValue,
                         float maxValue, const char* format) {
        label(text, fieldY);
        ImGui::SetCursorScreenPos(ImVec2(fieldX + U(96.0f), fieldY));
        ImGui::SetNextItemWidth(fieldWidth - U(96.0f));
        ImGui::SliderFloat(id, &value, minValue, maxValue, format);
        fieldY += rowHeight - U(2.0f);
    };
    sliderRow("Roughness", "##material_roughness", material.roughness, 0.02f, 1.0f, "%.2f");
    sliderRow("Metallic", "##material_metallic", material.metallic, 0.0f, 1.0f, "%.2f");
    sliderRow("Emissive", "##material_emissive", material.emissive, 0.0f, 4.0f, "%.2f");
    sliderRow("UV scale", "##material_uv", material.uvScale, 0.25f, 24.0f, "%.1f");

    // ---- actions -----------------------------------------------------------
    fieldY += U(10.0f);
    ImGui::SetCursorScreenPos(ImVec2(fieldX, fieldY));
    if (ui::PrimaryButton("Save", ImVec2(U(92.0f), U(28.0f)), true, ui::Icon::Save)) {
        if (library.Save(material)) {
            app.editor.materialStatus = "Saved " + material.name;
            app.SetFooterStatus(app.editor.materialStatus);
        }
    }
    ImGui::SameLine(0.0f, U(6.0f));
    if (ui::SecondaryButton("Assign to selection", ImVec2(U(150.0f), U(28.0f)), true)) {
        app.ApplyMaterialToSelection(material.name);
    }
    ImGui::SameLine(0.0f, U(6.0f));
    if (ui::SecondaryButton("Reload", ImVec2(U(84.0f), U(28.0f)), true)) {
        library.Scan(library.Directory());
        app.SetFooterStatus("Materials reloaded from disk");
    }

    // ---- footer -------------------------------------------------------------
    float footerY = y + listHeight + U(176.0f);
    dl->AddLine(ImVec2(left, footerY), ImVec2(left + listWidth + fieldWidth + U(16.0f), footerY),
                Colors().stroke, U(1.0f));
    footerY += U(10.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, footerY), Colors().textFaint,
                str::Ellipsis("HOT RELOAD  " + library.Directory(), 104).c_str());
    if (!app.editor.materialStatus.empty()) {
        dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, footerY + U(16.0f)),
                    Colors().success, str::Ellipsis(app.editor.materialStatus, 104).c_str());
    }

    ImGui::SetCursorScreenPos(ImVec2(left, footerY + U(30.0f)));
    ImGui::End();
    ImGui::PopStyleColor();
}

void WorldSettings(AstraApp& app) {
    if (!app.editor.showWorldSettings) return;
    ImGui::SetNextWindowSize(ImVec2(U(352.0f), U(600.0f)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x - U(392.0f), U(84.0f)),
                            ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("World Settings", &open)) {
        ImGui::End();
        if (!open) app.editor.showWorldSettings = false;
        return;
    }
    if (!open) app.editor.showWorldSettings = false;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    const float left = window->DC.CursorStartPos.x;
    float y = window->DC.CursorStartPos.y;
    const float rowHeight = U(27.0f);
    const float contentWidth = ImGui::GetContentRegionAvail().x;

    // The atmosphere lives on the sky actor; fall back to offering to create one.
    scene::Entity* sky = nullptr;
    for (scene::EntityId id : app.editor.scene.Roots()) {
        scene::Entity* entity = app.editor.scene.Find(id);
        if (entity && entity->Has(scene::Component::Atmosphere)) {
            sky = entity;
            break;
        }
    }

    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textFaint,
                "ATMOSPHERE");
    y += U(20.0f);
    if (sky) {
        auto& atmosphere = sky->atmosphere;
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().textDim,
                    str::Ellipsis(sky->name, 28).c_str());
        y += U(22.0f);
        LabeledFloat("Fog density", &atmosphere.fogDensity, 0.005f, 0.0f, 1.5f, "%.3f", y);
        y += rowHeight;
        LabeledFloat("Cloud cover", &atmosphere.cloudCover, 0.01f, 0.0f, 1.0f, "%.2f", y);
        y += rowHeight;
        LabeledFloat("Rain", &atmosphere.rainIntensity, 0.01f, 0.0f, 1.0f, "%.2f", y);
        y += rowHeight;
        LabeledFloat("Exposure", &atmosphere.exposure, 0.01f, 0.1f, 3.0f, "%.2f", y);
        y += rowHeight;

        dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y + U(9.0f)),
                    Colors().textDim, "Sun direction");
        ImGui::SetCursorScreenPos(ImVec2(left + U(150.0f), y + U(3.0f)));
        ImGui::SetNextItemWidth(U(140.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(6.0f));
        if (ImGui::DragFloat3("##sun_dir", &atmosphere.sunDirection.x, 0.01f, -1.0f, 1.0f, "%.2f")) {
            app.editor.scene.MarkDirty();
        }
        if (ImGui::IsItemActivated()) app.PushUndo("sun direction");
        ImGui::PopStyleVar();
        y += rowHeight + U(6.0f);

        if (ui::SecondaryButton("Reset to late afternoon", ImVec2(contentWidth, U(28.0f)))) {
            app.PushUndo("reset atmosphere");
            atmosphere.sunDirection = scene::Vec3(-0.35f, -0.72f, 0.60f);
            atmosphere.exposure = 1.05f;
            app.editor.scene.MarkDirty();
        }
        y += U(36.0f);
    } else {
        dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().textFaint,
                    "No atmosphere actor in this level.");
        y += U(24.0f);
        ImGui::SetCursorScreenPos(ImVec2(left, y));
        if (ui::SecondaryButton("Add Sky Atmosphere", ImVec2(contentWidth, U(28.0f)))) {
            app.PushUndo("add sky atmosphere");
            const scene::EntityId id = app.editor.scene.CreateEntity("Sky Atmosphere",
                                                                    scene::kInvalidEntity);
            if (scene::Entity* entity = app.editor.scene.Find(id)) {
                entity->Add(scene::Component::Atmosphere);
                entity->tag = "lighting";
            }
            app.editor.scene.MarkDirty();
        }
        y += U(38.0f);
    }

    dl->AddLine(ImVec2(left, y), ImVec2(left + contentWidth, y), Colors().stroke, U(1.0f));
    y += U(10.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textDim, "VIEWPORT");
    y += U(20.0f);
    CheckboxAt("Shadows", &app.editor.viewport.showShadows, ImVec2(left, y));
    CheckboxAt("Volumetrics", &app.editor.viewport.showVolumetrics, ImVec2(left + U(150.0f), y));
    y += U(26.0f);
    CheckboxAt("Grid", &app.editor.viewport.showGrid, ImVec2(left, y));
    CheckboxAt("Actor icons", &app.editor.viewport.showActorIcons, ImVec2(left + U(150.0f), y));
    y += U(32.0f);

    dl->AddLine(ImVec2(left, y), ImVec2(left + contentWidth, y), Colors().stroke, U(1.0f));
    y += U(10.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textDim, "RENDERING");
    y += U(20.0f);
    ImGui::SetCursorScreenPos(ImVec2(left, y));
    if (ui::SecondaryButton("Open path tracer", ImVec2(contentWidth, U(30.0f)), true, ui::Icon::Play)) {
        app.editor.showRenderPanel = true;
    }
    y += U(38.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textFaint,
                render::TraceBackendName(app.editor.trace.backend));
    y += U(20.0f);

    dl->AddLine(ImVec2(left, y), ImVec2(left + contentWidth, y), Colors().stroke, U(1.0f));
    y += U(10.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textDim, "BLENDER");
    y += U(20.0f);
    const int pending = static_cast<int>(asset::ScanInbox().size());
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(left, y), Colors().textDim,
                asset::BlenderStatus().c_str());
    y += U(20.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textFaint,
                str::Format("%d mesh(es) waiting in the inbox", pending).c_str());
    y += U(20.0f);
    ImGui::SetCursorScreenPos(ImVec2(left, y));
    if (ui::SecondaryButton("Import from Blender", ImVec2(contentWidth, U(30.0f)), pending > 0,
                            ui::Icon::Download)) {
        app.ImportFromBlender();
    }
    y += U(36.0f);
    y += U(18.0f);
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(left, y), Colors().textFaint,
                str::Format("%d actors  -  %d tris  -  %d particles", app.editor.viewportStats.actors,
                            app.editor.viewportStats.triangles, app.editor.viewportStats.particles)
                    .c_str());

    ImGui::End();
}

}  // namespace astra::app::panels
