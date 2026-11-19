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
    // Select the mesocyclone so the inspector shows a weather field.
    if (astra::scene::Entity* entity = app.editor.scene.FindByName("Supercell 03 / Mesocyclone")) {
        app.editor.selected = entity->id;
    }
    for (int i = 0; i < 6; ++i) RenderFrame([&] { app.Draw(); });
    Save("09-editor");
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

void ShotKeyForge(astra::app::KeyForgeApp& keyForge, bool unlocked) {
    keyForge.initialized = true;
    keyForge.ownerPath = astra::paths::Join(astra::paths::KeyForgeDataDir(), "owner.json");
    keyForge.ledger = astra::license::IssuedLedger::ForKeyForge();
    keyForge.ledger.Load();

    if (!unlocked) {
        keyForge.stage = astra::app::KeyForgeApp::Stage::CreatePassphrase;
        RenderFrame([&] { keyForge.Draw(); });
        RenderFrame([&] { keyForge.Draw(); });
        Save("13-keyforge-gate");
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
    Save("14-keyforge-generate");

    keyForge.tab = astra::app::KeyForgeApp::Tab::Ledger;
    keyForge.GenerateCodes();
    RenderFrame([&] { keyForge.Draw(); });
    RenderFrame([&] { keyForge.Draw(); });
    Save("15-keyforge-ledger");
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
    ShotSettings(app);
    ShotCommandPalette(app);

    astra::app::KeyForgeApp keyForge;
    ShotKeyForge(keyForge, false);
    ShotKeyForge(keyForge, true);

    astra::app::screens::Overlays(app);  // drain any pending overlays

    ImGui::DestroyContext();
    std::printf("done: %s\n", gPreview.outputDir.c_str());
    return 0;
}
