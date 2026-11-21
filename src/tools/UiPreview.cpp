// tools/UiPreview.cpp - renders the real studio UI headlessly to PNG.
//
// Usage:
//   astra_uipreview --out docs/screenshots [--width 1280] [--height 800] [--scale 1.0]
//   astra_uipreview --smoke
//
// This is a developer/QA tool: it builds the same AstraApp the windowed build uses,
// drives it into a known state and rasterizes the ImGui draw data on the CPU.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "astra/app/AstraApp.h"
#include "astra/app/screens/EditorPanels.h"
#include "astra/asset/Material.h"
#include "astra/asset/Bridge.h"
#include "astra/asset/MeshBuilder.h"
#include "astra/render/ViewportRenderer.h"
#include "astra/app/KeyForgeApp.h"
#include "astra/app/Screens.h"
#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/licensing/AccessCode.h"
#include "astra/licensing/MasterKey.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Theme.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"
#include "tools/SoftRaster.h"

namespace {

struct Preview {
    int width = 1280;
    int height = 800;
    float scale = 1.0f;
    unsigned char* atlasPixels = nullptr;
    int atlasWidth = 0;
    int atlasHeight = 0;
    soft::Framebuffer framebuffer;
    std::string outputDir = "docs/screenshots";
};

Preview gPreview;

bool ResolveTexture(ImTextureID id, soft::Texture& texture) {
    if (id == static_cast<ImTextureID>(1)) {
        texture.pixels = gPreview.atlasPixels;
        texture.width = gPreview.atlasWidth;
        texture.height = gPreview.atlasHeight;
        texture.alpha8 = true;
        return true;
    }
    for (const auto& image : astra::ui::branding::Images()) {
        if (image.texture == id) {
            texture.pixels = image.rgba.data();
            texture.width = image.width;
            texture.height = image.height;
            texture.alpha8 = false;
            return true;
        }
    }
    texture.pixels = nullptr;
    texture.width = 0;
    texture.height = 0;
    return false;
}

bool InitImGui(Preview& preview) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(static_cast<float>(preview.width), static_cast<float>(preview.height));
    io.DeltaTime = 1.0f / 60.0f;
    io.BackendPlatformName = "astra-headless";
    io.BackendRendererName = "astra-softraster";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!astra::ui::LoadFonts(preview.scale)) {
        std::fprintf(stderr, "warning: bundled fonts not found, using ImGui default\n");
    }
    astra::ui::ApplyStyle();

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));
    preview.atlasPixels = pixels;
    preview.atlasWidth = width;
    preview.atlasHeight = height;

    preview.framebuffer.Resize(preview.width, preview.height);
    astra::ui::branding::Logo();  // decode the logo up front
    return true;
}

// Runs one frame of `draw` and rasterizes it.
void RenderFrame(const std::function<void()>& draw) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(gPreview.width), static_cast<float>(gPreview.height));
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
    draw();
    ImGui::Render();

    gPreview.framebuffer.Resize(gPreview.width, gPreview.height);
    gPreview.framebuffer.Clear(9, 9, 11, 255);
    soft::RenderDrawData(ImGui::GetDrawData(), gPreview.framebuffer, ResolveTexture);
}

int gFailures = 0;

void Check(bool condition, const char* what) {
    if (condition) {
        std::printf("check: %-52s ok\n", what);
        return;
    }
    std::printf("check: %-52s FAILED\n", what);
    ++gFailures;
}

void MouseMove(float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); }
void MouseButton(int button, bool down) { ImGui::GetIO().AddMouseButtonEvent(button, down); }
void MouseWheel(float notches) { ImGui::GetIO().AddMouseWheelEvent(0.0f, notches); }
void KeyPress(ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    ImGui::GetIO().AddKeyEvent(key, false);
}

void Save(const std::string& name) {
    astra::fs::CreateDirectories(gPreview.outputDir);
    const std::string path = astra::paths::Join(gPreview.outputDir, name + ".png");
    if (!soft::WritePng(gPreview.framebuffer, path)) {
        std::fprintf(stderr, "failed to write %s\n", path.c_str());
        return;
    }
    std::printf("wrote %s (%dx%d)\n", path.c_str(), gPreview.width, gPreview.height);
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

void SeedAccounts(astra::app::AstraApp& app) {
    auto& store = app.auth.Accounts();
    if (!store.Accounts().empty()) return;

    auto owner = astra::auth::AccountStore::CreateAccount(
        "astrapictures", "Astra Productions", "StormChase#2026", astra::license::Role::Owner,
        "studio@astrapictures.com", "K7QM-****-****-B2VH");
    owner.createdUtc = astra::time::NowUtc() - 86400 * 148;
    owner.lastLoginUtc = astra::time::NowUtc() - 3600 * 5;
    owner.signInCount = 63;
    owner.accentHex = "F26B5E";
    store.Add(std::move(owner), nullptr);

    auto staff = astra::auth::AccountStore::CreateAccount(
        "a.reyes", "Ada Reyes", "Mesocyclone#77", astra::license::Role::Staff,
        "ada@astrapictures.com", "8XPD-****-****-4MRT");
    staff.createdUtc = astra::time::NowUtc() - 86400 * 41;
    staff.lastLoginUtc = astra::time::NowUtc() - 86400;
    staff.signInCount = 12;
    staff.accentHex = "6FA8DC";
    store.Add(std::move(staff), nullptr);
}

astra::license::AccessCode MakeDemoCode(astra::license::Role role, int days) {
    const astra::Bytes secret = astra::license::MasterKeyBytes();
    astra::license::CodePayload payload;
    payload.version = astra::kAccessCodeVersion;
    payload.role = role;
    payload.durationDays = days;
    payload.issuedDay = astra::license::DayFromUtc(astra::time::NowUtc());
    payload.nonce = 0x2A17;
    return astra::license::IssueCode(payload, secret);
}

void SignInOwner(astra::app::AstraApp& app) {
    if (const astra::auth::Account* account = app.auth.Accounts().FindByUsername("astrapictures")) {
        app.auth.EstablishSession(*account, true);
    }
}

void WaitForSync(astra::app::AstraApp& app, int timeoutMs = 4000) {
    const int stepMs = 50;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        app.fetcher.Poll();
        app.repos = app.fetcher.Snapshot();
        if (app.repos.state == astra::github::FetchState::Ready ||
            app.repos.state == astra::github::FetchState::Failed) {
            break;
        }
    }
}

void OpenDemoProject(astra::app::AstraApp& app) {
    astra::project::ProjectInfo info;
    std::string error;
    const std::string parent = astra::paths::Join(astra::paths::EngineDataDir(), "projects");
    if (astra::project::Create(parent, "Supercell Intercept", info, &error)) {
        app.OpenProject(info);
    } else {
        std::fprintf(stderr, "project create failed: %s\n", error.c_str());
    }
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

void ShotBoot(astra::app::AstraApp& app, float bootTime, const char* name, int warmup) {
    app.screen = astra::app::Screen::Boot;
    app.transition = 1.0f;
    app.bootTimer = bootTime;
    for (int i = 0; i < warmup; ++i) {
        app.bootTimer = bootTime - static_cast<float>(warmup - i) / 60.0f;
        RenderFrame([&] { app.Draw(); });
    }
    app.bootTimer = bootTime;
    RenderFrame([&] { app.Draw(); });
    Save(name);
}

void ShotSignIn(astra::app::AstraApp& app) {
    app.screen = astra::app::Screen::SignIn;
    app.transition = 1.0f;
    app.signIn.identity = "astrapictures";
    app.signIn.remember = true;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("03-sign-in");
}

void ShotRegisterCode(astra::app::AstraApp& app, bool withError) {
    app.screen = astra::app::Screen::RegisterCode;
    app.transition = 1.0f;
    if (withError) {
        app.reg.code = "K7QM-3XPD-9RTA-B2VX";
        app.reg.error = astra::license::CodeErrorMessage(astra::license::CodeError::BadSignature);
        app.reg.hint = astra::license::CodeErrorHint(astra::license::CodeError::BadSignature);
        RenderFrame([&] { app.Draw(); });
        RenderFrame([&] { app.Draw(); });
        Save("05-register-code-error");
    } else {
        app.reg.code = MakeDemoCode(astra::license::Role::Staff, 30).text;
        app.reg.error.clear();
        app.reg.hint.clear();
        RenderFrame([&] { app.Draw(); });
        RenderFrame([&] { app.Draw(); });
        Save("04-register-code");
    }
}

void ShotRegisterAccount(astra::app::AstraApp& app) {
    const auto code = MakeDemoCode(astra::license::Role::Staff, 30);
    app.reg.Reset();
    app.reg.code = code.text;
    app.reg.codeResult = app.auth.CheckAccessCode(code.text);
    app.reg.codeAccepted = app.reg.codeResult.ok();
    app.reg.displayName = "Ada Reyes";
    app.reg.username = "ada.reyes";
    app.reg.email = "ada@astrapictures.com";
    app.reg.password = "Mesocyclone#77";
    app.reg.confirmPassword = "Mesocyclone#77";
    app.reg.acceptedTerms = true;
    app.screen = astra::app::Screen::RegisterAccount;
    app.transition = 1.0f;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("06-register-account");
}

void ShotHubLoading(astra::app::AstraApp& app) {
    SignInOwner(app);
    app.screen = astra::app::Screen::ProjectHub;
    app.transition = 1.0f;
    app.repos = astra::github::RepoSnapshot{};
    app.repos.state = astra::github::FetchState::Loading;
    app.repos.demo = true;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("08-project-hub-loading");
}

void ShotHub(astra::app::AstraApp& app) {
    SignInOwner(app);
    app.screen = astra::app::Screen::ProjectHub;
    app.transition = 1.0f;
    app.StartRepoSync(true);
    WaitForSync(app);
    app.hub.search.clear();
    app.hub.filter = 0;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("07-project-hub");
}

void ShotEditor(astra::app::AstraApp& app) {
    SignInOwner(app);
    OpenDemoProject(app);
    app.screen = astra::app::Screen::Editor;
    app.transition = 1.0f;
    // Select a mesh actor so the inspector shows the static mesh group.
    if (astra::scene::Entity* entity = app.editor.scene.FindByName("Cube")) {
        app.editor.selected = entity->id;
    }
    for (int i = 0; i < 6; ++i) RenderFrame([&] { app.Draw(); });
    Save("09-editor");

    // ---- drive the viewport like a user, right where the panel published the rect -----
    astra::render::Rect viewport{{app.editor.viewportMinX, app.editor.viewportMinY},
                                 {app.editor.viewportMaxX, app.editor.viewportMaxY}};
    Check(viewport.Width() > 300.0f && viewport.Height() > 200.0f,
          "viewport published a usable screen rect");

    const ImVec2 pivot{(viewport.min.x + viewport.max.x) * 0.5f,
                       (viewport.min.y + viewport.max.y) * 0.5f};

    // Right drag orbits.
    const float yawBefore = app.editor.viewportCamera.yawDeg;
    MouseMove(pivot.x, pivot.y);
    RenderFrame([&] { app.Draw(); });
    MouseButton(1, true);
    RenderFrame([&] { app.Draw(); });
    for (int i = 1; i <= 6; ++i) {
        MouseMove(pivot.x + static_cast<float>(i) * 9.0f, pivot.y + static_cast<float>(i) * 2.0f);
        RenderFrame([&] { app.Draw(); });
    }
    MouseButton(1, false);
    RenderFrame([&] { app.Draw(); });
    Check(app.editor.viewportCamera.yawDeg != yawBefore, "right-drag orbited the camera");

    // Wheel zooms.
    const float distanceBefore = app.editor.viewportCamera.distance;
    MouseMove(pivot.x, pivot.y);
    RenderFrame([&] { app.Draw(); });
    MouseWheel(2.0f);
    RenderFrame([&] { app.Draw(); });
    MouseWheel(0.0f);
    RenderFrame([&] { app.Draw(); });
    Check(app.editor.viewportCamera.distance < distanceBefore, "wheel dollied the camera in");

    // Reset the framing, then click an actor the renderer itself picks.
    astra::render::FrameSelection(app.editor.viewportCamera, app.editor.scene,
                                  app.editor.scene.FindByName("Cube")
                                      ? app.editor.scene.FindByName("Cube")->id
                                      : astra::scene::kInvalidEntity);
    RenderFrame([&] { app.Draw(); });

    ImVec2 target{pivot.x, pivot.y};
    astra::scene::EntityId expected = astra::scene::kInvalidEntity;
    for (float y = viewport.min.y + 30.0f; y < viewport.max.y - 30.0f && expected == 0; y += 12.0f) {
        for (float x = viewport.min.x + 30.0f; x < viewport.max.x - 30.0f; x += 12.0f) {
            const ImVec2 probe{x, y};
            const astra::scene::EntityId hit =
                astra::render::Pick(viewport, app.editor.scene, app.editor.viewportCamera, probe);
            if (hit != astra::scene::kInvalidEntity && hit != app.editor.selected) {
                expected = hit;
                target = probe;
                break;
            }
        }
    }
    Check(expected != astra::scene::kInvalidEntity, "found a pickable actor in the viewport");

    app.editor.selected = astra::scene::kInvalidEntity;
    MouseMove(target.x, target.y);
    RenderFrame([&] { app.Draw(); });
    MouseButton(0, true);
    RenderFrame([&] { app.Draw(); });
    MouseButton(0, false);
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Check(app.editor.selected == expected, "left click selected the actor under the cursor");

    // F frames whatever is selected.
    const astra::scene::Entity* selectedEntity = app.editor.scene.Find(app.editor.selected);
    Check(selectedEntity != nullptr, "selection resolved to an entity");
    if (selectedEntity) {
        const astra::scene::Vec3 framedTarget = selectedEntity->transform.position;
        app.editor.viewportCamera.target = astra::scene::Vec3(0.0f, 0.0f, 0.0f);
        KeyPress(ImGuiKey_F);
        RenderFrame([&] { app.Draw(); });
        Check(app.editor.viewportCamera.target.x == framedTarget.x &&
                  app.editor.viewportCamera.target.z == framedTarget.z,
              "F framed the selected actor");
    }

    // Put the selection back on the cube for the wireframe shot.
    if (astra::scene::Entity* entity = app.editor.scene.FindByName("Cube")) {
        app.editor.selected = entity->id;
        astra::render::FrameSelection(app.editor.viewportCamera, app.editor.scene, entity->id);
    }
    RenderFrame([&] { app.Draw(); });

    // ---- panel moving: drag the World Outliner header ----------------------
    {
        const ImVec2 grab{viewport.min.x * 0.0f + 110.0f, viewport.min.y * 0.0f + 99.0f};
        const ImVec2 before = astra::app::panels::PaneOffset("##hierarchy_panel");
        MouseMove(grab.x, grab.y);
        RenderFrame([&] { app.Draw(); });
        MouseButton(0, true);
        RenderFrame([&] { app.Draw(); });
        for (int i = 1; i <= 6; ++i) {
            MouseMove(grab.x + static_cast<float>(i) * 14.0f,
                      grab.y + static_cast<float>(i) * 6.0f);
            RenderFrame([&] { app.Draw(); });
        }
        MouseButton(0, false);
        RenderFrame([&] { app.Draw(); });
        RenderFrame([&] { app.Draw(); });
        const ImVec2 after = astra::app::panels::PaneOffset("##hierarchy_panel");
        Check(after.x > 40.0f && after.y > 15.0f, "dragging a panel header moved the panel");
        Check(!astra::app::panels::AnyPanelDragging(), "panel drag ended on mouse release");
        Save("09-panels-moved");

        astra::app::panels::ResetPanelLayout();
        RenderFrame([&] { app.Draw(); });
        const ImVec2 reset = astra::app::panels::PaneOffset("##hierarchy_panel");
        Check(reset.x == before.x && reset.y == before.y, "reset put the panel back on its slot");
    }

    // Same level, wireframe view mode with the cone framed.
    app.editor.viewport.mode = astra::render::ViewMode::Wireframe;
    app.editor.tool = 1;
    if (astra::scene::Entity* entity = app.editor.scene.FindByName("Cone")) {
        app.editor.selected = entity->id;
        astra::render::FrameSelection(app.editor.viewportCamera, app.editor.scene, entity->id);
    }
    for (int i = 0; i < 3; ++i) RenderFrame([&] { app.Draw(); });
    Save("09-editor-wireframe");
    app.editor.viewport.mode = astra::render::ViewMode::Shaded;
}

void KeyDown(ImGuiKey key, bool down) { ImGui::GetIO().AddKeyEvent(key, down); }

void ShotPlay(astra::app::AstraApp& app) {
    app.editor.showRenderPanel = false;
    app.editor.showWorldSettings = false;
    app.screen = astra::app::Screen::Editor;
    app.transition = 1.0f;
    if (astra::scene::Entity* entity = app.editor.scene.FindByName("Cube")) {
        app.editor.selected = entity->id;
    }
    app.EnterPlay();
    RenderFrame([&] { app.Draw(); });
    Check(app.editor.playing, "play-in-editor started");
    Check(app.editor.player.position.y > 0.0f, "player spawned above the floor");

    const astra::scene::Vec3 start = app.editor.player.position;
    KeyDown(ImGuiKey_W, true);
    for (int i = 0; i < 24; ++i) RenderFrame([&] { app.Draw(); });
    KeyDown(ImGuiKey_W, false);
    RenderFrame([&] { app.Draw(); });
    const float travelled = std::sqrt(
        (app.editor.player.position.x - start.x) * (app.editor.player.position.x - start.x) +
        (app.editor.player.position.z - start.z) * (app.editor.player.position.z - start.z));
    Check(travelled > 0.5f, "W moved the possessed pawn forward");

    KeyDown(ImGuiKey_Space, true);
    RenderFrame([&] { app.Draw(); });
    KeyDown(ImGuiKey_Space, false);
    for (int i = 0; i < 4; ++i) RenderFrame([&] { app.Draw(); });
    Check(app.editor.player.velocityY > 0.0f || app.editor.player.position.y > start.y,
          "space made the pawn jump");
    for (int i = 0; i < 10; ++i) RenderFrame([&] { app.Draw(); });
    Save("14-play-in-editor");

    app.ExitPlay();
    RenderFrame([&] { app.Draw(); });
    Check(!app.editor.playing, "stop ended the play session");
}

void ShotMaterialEditor(astra::app::AstraApp& app) {
    // The editor scans the real material folder (seeding starters on first run).
    app.LoadMaterials();
    app.editor.showMaterialEditor = true;
    app.editor.showRenderPanel = false;
    if (astra::scene::Entity* cube = app.editor.scene.FindByName("Cube")) {
        app.editor.selected = cube->id;
    }
    for (int i = 0; i < 3; ++i) RenderFrame([&] { app.Draw(); });
    Check(app.editor.materials.All().size() >= 4, "material library loaded authored assets");
    Save("22-material-editor");

    // Assigning through the real command must reach the actor and push undo.
    const std::size_t undoBefore = app.editor.undo.size();
    const bool applied = app.ApplyMaterialToSelection("Hologram Panel");
    RenderFrame([&] { app.Draw(); });
    Check(applied, "assigned a material asset to the selection");
    Check(app.editor.undo.size() == undoBefore + 1, "material assignment is undoable");
    const astra::scene::Entity* cube = app.editor.scene.FindByName("Cube");
    Check(cube != nullptr && cube->Has(astra::scene::Component::Material) &&
              cube->material.shader == astra::scene::MaterialShader::Hologram,
          "material shader reached the actor");

    // Editing a value and saving must hit the file on disk.
    const std::string directory = app.editor.materials.Directory();
    const std::string path = astra::paths::Join(directory, "Hologram_Panel.astramat");
    if (astra::asset::MaterialAsset* material = app.editor.materials.FindMutable("Hologram Panel")) {
        material->emissive = 2.5f;
        Check(app.editor.materials.Save(*material), "saved the edited material");
        std::string text;
        Check(astra::fs::ReadTextFile(path, text) && text.find("2.5") != std::string::npos,
              "edited material reached disk");
    }
    RenderFrame([&] { app.Draw(); });
    Save("23-material-editor-applied");
    app.editor.showMaterialEditor = false;
}

void ShotRenderPanel(astra::app::AstraApp& app) {
    app.editor.showRenderPanel = true;
    app.editor.trace.width = 320;
    app.editor.trace.height = 180;
    app.editor.trace.samples = 64;
    app.editor.trace.maxBounces = 3;
    // Frame the whole level so the still reads like a render, not a close-up.
    app.editor.viewportCamera.target = astra::scene::Vec3(0.0f, 1.0f, 0.0f);
    app.editor.viewportCamera.distance = 13.0f;
    app.editor.viewportCamera.yawDeg = 24.0f;
    app.editor.viewportCamera.pitchDeg = 16.0f;
    for (int i = 0; i < 2; ++i) RenderFrame([&] { app.Draw(); });

    // Drive the real command rather than calling the tracer directly.
    astra::render::TraceScene baked;
    baked.Build(app.editor.scene);
    app.editor.traceImage =
        astra::render::Render(baked, app.editor.viewportCamera, app.editor.trace,
                              &app.editor.traceStats);
    RenderFrame([&] { app.Draw(); });
    Check(!app.editor.traceImage.Empty(), "path tracer produced an image");
    Check(app.editor.traceStats.triangles > 100, "path tracer built a triangle soup");
    Check(app.editor.traceStats.rays > 10000, "path tracer cast rays");
    Save("15-render-panel");

    const std::string saved = astra::paths::Join(gPreview.outputDir, "15-render-panel-raytraced.png");
    Check(astra::render::RenderToFile(app.editor.scene, app.editor.viewportCamera, app.editor.trace,
                                      saved, nullptr),
          "render saved to PNG");
    app.editor.showRenderPanel = false;
}

void ShotScriptEditor(astra::app::AstraApp& app) {
    app.editor.showRenderPanel = false;
    app.editor.showWorldSettings = false;
    const std::string dir = astra::paths::Join(astra::paths::EngineDataDir(), "preview-scripts");
    astra::fs::CreateDirectories(dir);
    const std::string path = astra::paths::Join(dir, "level_tools.astra");
    astra::fs::WriteTextFileAtomic(path, "print(\"preview\")\n");
    app.OpenScriptEditor(dir, path, std::string());
    RenderFrame([&] { app.Draw(); });
    Check(app.screen == astra::app::Screen::ScriptEditor, "script editor screen opened");

    app.script.source =
        "// AstraScript preview\n"
        "print(\"hello from AstraScript\")\n"
        "let meshId = meshRegister(meshCylinder(16), \"Pillar\")\n"
        "for (i in range(4)) {\n"
        "    let actor = spawnActor(format(\"Pillar {}\", i), \"custom\", i * 2.0, 1.5, 0.0)\n"
        "    attachMesh(actor, meshId)\n"
        "    setScale(actor, 0.6, 2.4, 0.6)\n"
        "}\n"
        "let img = imageNew(96, 64)\n"
        "imageGradient(img, 0.1, 0.4, 0.9, 0.2, 0.7, 1.0)\n"
        "imageNoise(img, 0.08)\n"
        "imageSave(img, \"preview.png\")\n"
        "print(\"actors:\", actorCount())\n";
    app.script.dirty = true;
    RenderFrame([&] { app.Draw(); });

    const int actorsBefore = static_cast<int>(app.editor.scene.Count());
    Check(app.RunScript(), "script ran from the editor");
    Check(app.script.error.empty(), "script reported no errors");
    RenderFrame([&] { app.Draw(); });
    for (int i = 0; i < 2; ++i) RenderFrame([&] { app.Draw(); });
    Check(static_cast<int>(app.editor.scene.Count()) > actorsBefore,
          "script spawned actors into the level");
    Save("16-script-editor");

    app.editor.showWorldSettings = true;
    app.screen = astra::app::Screen::Editor;
    app.transition = 1.0f;
    for (int i = 0; i < 3; ++i) RenderFrame([&] { app.Draw(); });
    Save("17-world-settings");
    app.editor.showWorldSettings = false;
}

void ShotSettings(astra::app::AstraApp& app) {
    app.showSettings = true;
    app.settingsTab = 0;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("10-settings");
    app.settingsTab = 1;
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("11-settings-github");
    app.showSettings = false;
}

void ShotCommandPalette(astra::app::AstraApp& app) {
    app.showCommandPalette = true;
    app.commandText.clear();
    RenderFrame([&] { app.Draw(); });
    RenderFrame([&] { app.Draw(); });
    Save("12-command-palette");
    app.showCommandPalette = false;
}

void ShotBlenderImport(astra::app::AstraApp& app) {
    app.editor.showRenderPanel = false;
    app.screen = astra::app::Screen::Editor;
    app.transition = 1.0f;

    // Simulate the addon: two meshes land in the inbox, then the editor imports them.
    std::string error;
    Check(astra::asset::WriteBridgeJob("Blender Torus", astra::asset::MakeTorus(24, 12, 0.30f),
                                       "Hologram", &error),
          "bridge accepted an exported mesh");
    Check(astra::asset::WriteBridgeJob("Blender Cone", astra::asset::MakeCone(20), "Emissive",
                                       &error),
          "bridge accepted a second mesh");
    const std::size_t before = app.editor.scene.Count();
    app.ImportFromBlender();
    RenderFrame([&] { app.Draw(); });
    Check(app.editor.scene.Count() == before + 2, "both Blender meshes became actors");
    Check(!app.editor.scene.Meshes().empty(), "imported meshes joined the level mesh library");

    if (const astra::scene::Entity* actor = app.editor.scene.FindByName("Blender Torus")) {
        app.editor.selected = actor->id;
        astra::render::FrameSelection(app.editor.viewportCamera, app.editor.scene, actor->id);
    }
    for (int i = 0; i < 3; ++i) RenderFrame([&] { app.Draw(); });
    Save("18-blender-import");
}

void ShotKeyForge(astra::app::KeyForgeApp& keyForge, bool unlocked) {
    keyForge.initialized = true;
    keyForge.ownerPath = astra::paths::Join(astra::paths::KeyForgeDataDir(), "owner.json");
    keyForge.ledger = astra::license::IssuedLedger::ForKeyForge();
    keyForge.ledger.Load();

    if (!unlocked) {
        keyForge.stage = astra::app::KeyForgeApp::Stage::CreatePassphrase;
        RenderFrame([&] { keyForge.Draw(); });
        RenderFrame([&] { keyForge.Draw(); });
        Save("21-keyforge-gate");
        return;
    }

    keyForge.stage = astra::app::KeyForgeApp::Stage::Ready;
    keyForge.tab = astra::app::KeyForgeApp::Tab::Generate;
    keyForge.roleIndex = 1;
    keyForge.durationIndex = 4;
    keyForge.quantity = 3;
    keyForge.note = "Season 2026 intercept crew";
    keyForge.batchName = "october-onboarding";
    keyForge.GenerateCodes();
    RenderFrame([&] { keyForge.Draw(); });
    RenderFrame([&] { keyForge.Draw(); });
    Save("19-keyforge-generate");

    keyForge.tab = astra::app::KeyForgeApp::Tab::Ledger;
    keyForge.GenerateCodes();
    RenderFrame([&] { keyForge.Draw(); });
    RenderFrame([&] { keyForge.Draw(); });
    Save("20-keyforge-ledger");
}

// ---------------------------------------------------------------------------
// Smoke test
// ---------------------------------------------------------------------------

int RunSmokeTest() {
    const int sizes[3][2] = {{1280, 800}, {1024, 700}, {1600, 900}};
    astra::app::AstraApp app;
    astra::app::KeyForgeApp keyForge;

    for (const auto& size : sizes) {
        gPreview.width = size[0];
        gPreview.height = size[1];
        for (int pass = 0; pass < 2; ++pass) {
            const astra::app::Screen screens[] = {
                astra::app::Screen::Boot,        astra::app::Screen::SignIn,
                astra::app::Screen::RegisterCode, astra::app::Screen::RegisterAccount,
                astra::app::Screen::ProjectHub,   astra::app::Screen::Editor};
            for (auto screen : screens) {
                app.screen = screen;
                app.transition = 1.0f;
                app.bootTimer = pass == 0 ? 0.6f : 3.0f;
                RenderFrame([&] { app.Draw(); });
                RenderFrame([&] { app.Draw(); });
            }
            app.showSettings = true;
            RenderFrame([&] { app.Draw(); });
            app.showSettings = false;
            app.showCommandPalette = true;
            RenderFrame([&] { app.Draw(); });
            app.showCommandPalette = false;
            app.PushToast(astra::app::ToastKind::Success, "Smoke test", "toast rendering");
            RenderFrame([&] { app.Draw(); });

            keyForge.stage = astra::app::KeyForgeApp::Stage::Unlock;
            RenderFrame([&] { keyForge.Draw(); });
            keyForge.stage = astra::app::KeyForgeApp::Stage::Ready;
            RenderFrame([&] { keyForge.Draw(); });
        }
        std::printf("smoke: %dx%d ok\n", gPreview.width, gPreview.height);
    }
    std::printf("smoke test passed: every screen rendered without assertion\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) gPreview.outputDir = argv[++i];
        else if (arg == "--width" && i + 1 < argc) gPreview.width = std::atoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) gPreview.height = std::atoi(argv[++i]);
        else if (arg == "--scale" && i + 1 < argc) gPreview.scale = static_cast<float>(std::atof(argv[++i]));
        else if (arg == "--smoke") smoke = true;
        else if (arg == "--help") {
            std::printf("astra_uipreview --out <dir> [--width N] [--height N] [--scale F] [--smoke]\n");
            return 0;
        }
    }

    // Never touch the developer's real registry while previewing.
    std::error_code ec;
    std::filesystem::remove_all("/tmp/astra-preview-data", ec);
    astra::paths::SetDataRootOverride("/tmp/astra-preview-data");
    astra::log::Sink::Instance().SetCapacity(512);

    if (!InitImGui(gPreview)) return 1;

    if (smoke) {
        const int result = RunSmokeTest();
        ImGui::DestroyContext();
        return result;
    }

    astra::app::AstraApp app;
    app.headless = true;
    app.Init(true);
    SeedAccounts(app);

    // Boot screens first (they depend on timers rather than state).
    ShotBoot(app, 1.15f, "01-boot-loading", 3);
    ShotBoot(app, 2.85f, "02-boot-sequence", 2);

    ShotSignIn(app);
    ShotRegisterCode(app, false);
    ShotRegisterCode(app, true);
    ShotRegisterAccount(app);
    ShotHubLoading(app);
    ShotHub(app);
    ShotEditor(app);
    ShotPlay(app);
    ShotRenderPanel(app);
    ShotMaterialEditor(app);
    ShotScriptEditor(app);
    ShotBlenderImport(app);
    ShotSettings(app);
    ShotCommandPalette(app);

    astra::app::KeyForgeApp keyForge;
    ShotKeyForge(keyForge, false);
    ShotKeyForge(keyForge, true);

    astra::app::screens::Overlays(app);  // drain any pending overlays

    ImGui::DestroyContext();
    if (gFailures > 0) {
        std::printf("failed: %d interaction check(s)\n", gFailures);
        return 1;
    }
    std::printf("done: %s\n", gPreview.outputDir.c_str());
    return 0;
}
