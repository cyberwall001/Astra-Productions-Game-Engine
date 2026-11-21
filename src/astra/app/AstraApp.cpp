#include "astra/app/AstraApp.h"

#include <algorithm>
#include <cmath>

#include "astra/app/Screens.h"
#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/core/Version.h"
#include "astra/crypto/SecretProtect.h"
#include "astra/ui/Theme.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app {
namespace {

constexpr float kToastLifetime = 5.0f;

std::string WorkspaceRoot() { return paths::Join(paths::EngineDataDir(), "workspace"); }

}  // namespace

void AstraApp::Init(bool headlessMode) {
    headless = headlessMode;
    launchMs = time::NowMonotonicMs();

    settings.Load();
    auth.Initialize();
    recents.Load();

    log::Sink::Instance().SetMinimumLevel(settings.Get().verboseLogging ? log::Level::Debug
                                                                       : log::Level::Info);

    bootStageCount = static_cast<int>(boot::Stages().size());
    editor.scene = scene::Scene::CreateSampleLevel();
    editor.selected = editor.scene.Roots().empty() ? scene::kInvalidEntity
                                                   : editor.scene.Roots().front();
    editor.frameHistory.reserve(120);

    log::Info("studio", std::string("Astra Studio ") + FullVersionString() + " starting");
    log::Info("studio", "data root: " + paths::DataRoot());
    log::Info("studio", "asset root: " + paths::AssetRoot());

    if (settings.Get().skipBootScreen) {
        screen = Screen::SignIn;
        transition = 1.0f;
        bootTimer = boot::TotalDuration();
    }
    initialized = true;
}

void AstraApp::Shutdown() {
    fetcher.Shutdown();
    settings.Save();
}

void AstraApp::Goto(Screen next, bool instant) {
    if (screen == next) return;
    screen = next;
    transition = instant ? 1.0f : 0.0f;
    log::Debug("studio", std::string("screen -> ") + std::to_string(static_cast<int>(next)));
}

void AstraApp::PushToast(ToastKind kind, std::string title, std::string body) {
    Toast toast;
    toast.kind = kind;
    toast.title = std::move(title);
    toast.body = std::move(body);
    toasts.push_back(std::move(toast));
    if (toasts.size() > 4) toasts.erase(toasts.begin());

    switch (kind) {
        case ToastKind::Error: log::Error("studio", title); break;
        case ToastKind::Warning: log::Warn("studio", title); break;
        default: log::Info("studio", title); break;
    }
}

void AstraApp::SetFooterStatus(std::string text) {
    footerStatus = std::move(text);
    footerStatusTimer = 4.0f;
}

void AstraApp::Tick(float deltaTime) {
    dt = ImClamp(deltaTime, 0.0f, 0.1f);
    time += dt;
    transition = ImMin(1.0f, transition + dt / 0.28f);
    footerStatusTimer = ImMax(0.0f, footerStatusTimer - dt);

    for (auto& toast : toasts) toast.age += dt;
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
                                [](const Toast& t) { return t.age >= t.ttl; }),
                 toasts.end());

    // Network results arrive on a worker thread; publish them on the UI thread.
    if (fetcher.Poll()) {
        repos = fetcher.Snapshot();
        if (repos.state == github::FetchState::Failed) {
            PushToast(ToastKind::Error, "GitHub sync failed", repos.error);
        } else if (repos.state == github::FetchState::Ready) {
            SetFooterStatus(str::Format("Synced %zu repositories", repos.repos.size()));
        }
    }

    switch (screen) {
        case Screen::Boot: {
            bootTimer += dt;
            bootStage = 0;
            const auto& stages = boot::Stages();
            for (std::size_t i = 0; i < stages.size(); ++i) {
                if (bootTimer >= stages[i].at) bootStage = static_cast<int>(i);
            }
            if (bootTimer >= boot::TotalDuration()) {
                if (!autoSignInChecked) {
                    autoSignInChecked = true;
                    if (settings.Get().rememberAccount) {
                        const auth::SignInResult restored = auth.RestoreSession();
                        if (restored.ok && restored.account) {
                            PushToast(ToastKind::Success, "Welcome back, " + restored.account->displayName,
                                  "Restored your saved session.");
                            Goto(Screen::ProjectHub);
                            StartRepoSync();
                            break;
                        }
                    }
                }
                Goto(Screen::SignIn);
            }
            break;
        }
        case Screen::Editor: {
            auto& history = editor.frameHistory;
            const float frameMs = dt * 1000.0f;
            if (history.empty()) {
                history.assign(120, frameMs);
            } else {
                history.erase(history.begin());
                history.push_back(frameMs);
            }
            if (editor.statusTimer > 0.0f) editor.statusTimer -= dt;
            if (editor.autosave) {
                editor.autosaveTimer += dt;
                if (editor.autosaveTimer > 90.0f && editor.scene.IsDirty() &&
                    editor.project.isValid()) {
                    editor.autosaveTimer = 0.0f;
                    SaveScene();
                    SetFooterStatus("Autosaved scene");
                }
            }
            break;
        }
        default: break;
    }
}

void AstraApp::Draw() { DrawAstraApp(*this); }

// ---------------------------------------------------------------------------
// Auth flows
// ---------------------------------------------------------------------------

void AstraApp::SignInUser(const auth::Account& account) {
    auth.EstablishSession(account, signIn.remember && settings.Get().rememberAccount);
    signIn.password.clear();
    signIn.error.clear();
    signIn.hint.clear();
    PushToast(ToastKind::Success, "Signed in as " + account.displayName,
          std::string(license::RoleName(account.role)) + " access");
    Goto(Screen::ProjectHub);
    StartRepoSync();
}

void AstraApp::SubmitSignIn() {
    const auth::SignInResult result =
        auth.SignIn(signIn.identity, signIn.password, signIn.remember && settings.Get().rememberAccount);
    if (!result.ok) {
        signIn.error = result.error;
        signIn.hint = result.hint;
        signIn.attempts += 1;
        signIn.password.clear();
        return;
    }
    signIn.attempts = 0;
    if (result.account) SignInUser(*result.account);
}

void AstraApp::SignOut() {
    auth.SignOut();
    editor.project = project::ProjectInfo{};
    signIn.password.clear();
    signIn.error.clear();
    signIn.hint.clear();
    Goto(Screen::SignIn);
    PushToast(ToastKind::Info, "Signed out", "Your saved session was cleared.");
}

void AstraApp::SubmitCode() {
    reg.codeResult = auth.CheckAccessCode(reg.code);
    reg.codeAccepted = reg.codeResult.ok();
    if (!reg.codeAccepted) {
        reg.error = license::CodeErrorMessage(reg.codeResult.error);
        reg.hint = license::CodeErrorHint(reg.codeResult.error);
        return;
    }
    reg.error.clear();
    reg.hint.clear();
    PushToast(ToastKind::Success, "Access code verified", reg.codeResult.code.Description());
    Goto(Screen::RegisterAccount);
}

void AstraApp::SubmitRegistration() {
    const auth::RegisterResult result =
        auth.Register(reg.code, reg.username, reg.displayName, reg.password, reg.confirmPassword,
                      reg.email, reg.acceptedTerms);
    if (!result.ok) {
        reg.error = result.error;
        reg.hint = result.hint;
        return;
    }

    const auth::Account* account = auth.Current();
    const std::string name = account ? account->displayName : reg.displayName;
    reg.Reset();
    PushToast(ToastKind::Success, "Welcome to Astra, " + name,
          "Your engine account is registered on this machine.");
    Goto(Screen::ProjectHub);
    StartRepoSync();
}

// ---------------------------------------------------------------------------
// GitHub
// ---------------------------------------------------------------------------

std::string AstraApp::GitHubToken() const {
    return crypto::UnprotectSecret(settings.Get().githubTokenEncoded);
}

void AstraApp::SetGitHubToken(const std::string& token, const std::string& login) {
    AppSettings& mutableSettings = settings.Get();
    mutableSettings.githubTokenEncoded = crypto::ProtectSecret(token);
    mutableSettings.githubLogin = login;
    settings.Save();
    StartRepoSync();
}

void AstraApp::StartRepoSync(bool forceDemo) {
    const std::string token = GitHubToken();
    const bool demo = forceDemo || (token.empty() && settings.Get().allowDemoWorkspace);
    fetcher.Start(demo ? std::string() : token, demo);
    hub.connecting = true;
    if (demo) {
        log::Info("github", "starting demo workspace sync (no token configured)");
    } else {
        log::Info("github", "starting GitHub sync with stored token");
    }
}

void AstraApp::OpenRepo(const github::Repo& repo) {
    if (!repo.hasAstraProject) {
        PushToast(ToastKind::Warning, "No Astra project in " + repo.name,
              "Add an astra.project.json at the repository root to open it here.");
        return;
    }

    const std::string dir = paths::Join(WorkspaceRoot(), str::SanitizeFileName(repo.name));
    project::ProjectInfo info;
    if (!project::Read(dir, info)) {
        info.name = repo.name;
        info.rootPath = dir;
        info.projectFilePath = paths::Join(dir, project::kProjectFileName);
        info.engineVersion = kEngineVersion;
        info.repoFullName = repo.fullName;
        info.description = repo.description;
        info.defaultScene = "scenes/" + str::SanitizeFileName(repo.name) + ".astra.scene";
        project::Write(info);

        scene::Scene starter = scene::Scene::CreateSampleLevel(repo.name);
        project::SaveScene(info, starter);
    }

    info.repoFullName = repo.fullName;
    info.lastOpenedUtc = time::NowUtc();
    OpenProject(info);
    PushToast(ToastKind::Success, "Opened " + repo.name,
          repo.fullName + " linked as a local workspace");
}

void AstraApp::OpenProject(const project::ProjectInfo& info) {
    editor.project = info;
    editor.selected = scene::kInvalidEntity;
    std::string error;
    if (!project::LoadScene(info, editor.scene, &error)) {
        log::Warn("project", error);
        editor.scene = scene::Scene::CreateSampleLevel(info.name);
        project::SaveScene(info, editor.scene);
    }
    const auto roots = editor.scene.Roots();
    editor.selected = roots.empty() ? scene::kInvalidEntity : roots.front();
    editor.frameHistory.clear();
    recents.Touch(info);
    settings.Get().activeProjectPath = info.rootPath;
    settings.Save();
    Goto(Screen::Editor);
    log::Info("project", "opened " + info.name + " at " + info.rootPath);
}

void AstraApp::CreateProject(const std::string& name) {
    project::ProjectInfo info;
    std::string error;
    const std::string parent = paths::Join(paths::EngineDataDir(), "projects");
    if (!project::Create(parent, name, info, &error)) {
        PushToast(ToastKind::Error, "Could not create project", error);
        return;
    }
    OpenProject(info);
    PushToast(ToastKind::Success, "Project created", info.rootPath);
}

void AstraApp::CloseProject() {
    if (editor.scene.IsDirty() && editor.project.isValid()) SaveScene();
    editor.project = project::ProjectInfo{};
    Goto(Screen::ProjectHub);
}

void AstraApp::SaveScene() {
    if (!editor.project.isValid()) {
        PushToast(ToastKind::Warning, "No project open", "Create or open a project to save scenes.");
        return;
    }
    std::string error;
    if (!project::SaveScene(editor.project, editor.scene, &error)) {
        PushToast(ToastKind::Error, "Scene save failed", error);
        return;
    }
    editor.scene.ClearDirty();
    editor.statusText = "Scene saved " + time::FormatUtc(time::NowUtc(), false);
    editor.statusTimer = 3.0f;
    SetFooterStatus("Saved " + editor.scene.Name());
}

void AstraApp::NewScene() {
    if (!editor.project.isValid()) return;
    editor.scene = scene::Scene::CreateSampleLevel("Untitled Level");
    editor.selected = editor.scene.Roots().empty() ? scene::kInvalidEntity
                                                   : editor.scene.Roots().front();
    editor.scene.MarkDirty();
    SetFooterStatus("New scene created");
}

void AstraApp::RefreshDemoContent() {
    editor.scene = scene::Scene::CreateSampleLevel();
    editor.selected = editor.scene.Roots().empty() ? scene::kInvalidEntity
                                                   : editor.scene.Roots().front();
    SetFooterStatus("Sample scene rebuilt");
}

}  // namespace astra::app
