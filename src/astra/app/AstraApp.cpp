#include "astra/app/AstraApp.h"

#include "astra/asset/Bridge.h"

#include <algorithm>
#include <cmath>
#include <memory>

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
    const std::string dir = paths::Join(WorkspaceRoot(), str::SanitizeFileName(repo.name));

    // No astra.project.json? The repository is still worth opening: land in the script
    // workspace, point it at any scripts the repo ships, and offer to convert it into a level.
    if (!repo.hasAstraProject) {
        fs::CreateDirectories(dir);
        std::string scripts = paths::Join(dir, "Scripts");
        if (!fs::IsDirectory(scripts)) scripts = dir;
        OpenScriptEditor(scripts, std::string(), repo.name);
        PushToast(ToastKind::Info, "Script workspace: " + repo.name,
                  "No astra.project.json here, so scripts open directly. Save one as a level "
                  "tool, or press Build to scaffold a project.");
        log::Info("project", "opened non-Astra repo in the script editor: " + repo.fullName);
        return;
    }

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

// ---------------------------------------------------------------------------
// Undo / redo: whole-level snapshots, pushed before a command mutates the scene.
// ---------------------------------------------------------------------------
void AstraApp::PushUndo(std::string label) {
    UndoEntry entry;
    entry.label = std::move(label);
    entry.sceneJson = editor.scene.ToJson();
    editor.undo.push_back(std::move(entry));
    if (static_cast<int>(editor.undo.size()) > editor.undoLimit) {
        editor.undo.erase(editor.undo.begin());
    }
    editor.redo.clear();
}

void AstraApp::Undo() {
    if (editor.undo.empty()) {
        SetFooterStatus("Nothing to undo");
        return;
    }
    UndoEntry entry = editor.undo.back();
    editor.undo.pop_back();

    UndoEntry current;
    current.label = entry.label;
    current.sceneJson = editor.scene.ToJson();
    editor.redo.push_back(std::move(current));

    std::string error;
    if (!editor.scene.FromJson(entry.sceneJson, &error)) {
        PushToast(ToastKind::Error, "Undo failed", error);
        return;
    }
    if (!editor.scene.Find(editor.selected)) editor.selected = scene::kInvalidEntity;
    editor.scene.MarkDirty();
    SetFooterStatus("Undo: " + entry.label);
}

void AstraApp::Redo() {
    if (editor.redo.empty()) {
        SetFooterStatus("Nothing to redo");
        return;
    }
    UndoEntry entry = editor.redo.back();
    editor.redo.pop_back();

    UndoEntry current;
    current.label = entry.label;
    current.sceneJson = editor.scene.ToJson();
    editor.undo.push_back(std::move(current));

    std::string error;
    if (!editor.scene.FromJson(entry.sceneJson, &error)) {
        PushToast(ToastKind::Error, "Redo failed", error);
        return;
    }
    if (!editor.scene.Find(editor.selected)) editor.selected = scene::kInvalidEntity;
    editor.scene.MarkDirty();
    SetFooterStatus("Redo: " + entry.label);
}

void AstraApp::DuplicateSelected() {
    const scene::Entity* source = editor.scene.Find(editor.selected);
    if (!source) {
        SetFooterStatus("Select an actor to duplicate");
        return;
    }
    PushUndo("duplicate " + source->name);
    const scene::EntityId copyId = editor.scene.CreateEntity(source->name + " copy", source->parent);
    if (scene::Entity* copy = editor.scene.Find(copyId)) {
        const std::string name = copy->name;
        *copy = *source;
        copy->id = copyId;
        copy->name = name;
        copy->parent = source->parent;
        copy->children.clear();
        copy->transform.position.x += 1.2f;
        copy->transform.position.z += 1.2f;
    }
    editor.selected = copyId;
    SetFooterStatus("Duplicated " + source->name);
}

void AstraApp::DeleteSelected() {
    if (editor.selected == scene::kInvalidEntity) return;
    const scene::Entity* entity = editor.scene.Find(editor.selected);
    if (!entity) return;
    const std::string name = entity->name;
    PushUndo("delete " + name);
    editor.scene.DestroyEntity(editor.selected);
    editor.selected = scene::kInvalidEntity;
    SetFooterStatus("Deleted " + name);
}

// ---------------------------------------------------------------------------
// Play in editor
// ---------------------------------------------------------------------------
void AstraApp::EnterPlay() {
    if (editor.playing) return;
    if (!editor.playerSpawned) {
        game::SpawnFromLevel(editor.player, editor.scene);
        editor.playerSpawned = true;
    }
    editor.playing = true;
    editor.simulating = false;
    editor.playTime = 0.0f;
    editor.viewport.mode = render::ViewMode::Shaded;
    log::Info("editor", "play-in-editor session started");
    SetFooterStatus("Playing - WASD to move, right drag to look, Esc to stop");
}

void AstraApp::ExitPlay() {
    if (!editor.playing) return;
    editor.playing = false;
    editor.playTime = 0.0f;
    SetFooterStatus("Play stopped");
}

// ---------------------------------------------------------------------------
// Scripting
// ---------------------------------------------------------------------------
void AstraApp::RefreshScriptFiles() {
    script.files.clear();
    if (script.directory.empty()) return;
    for (const auto& entry : fs::ListDirectory(script.directory)) {
        const std::string full = paths::Join(script.directory, entry);
        if (fs::IsDirectory(full)) {
            // One level deep is enough for script folders and keeps the list readable.
            for (const auto& child : fs::ListDirectory(full)) {
                const std::string extension = child.size() > 6 ? child.substr(child.size() - 6) : "";
                if (extension == ".astra") script.files.push_back(entry + "/" + child);
            }
            continue;
        }
        if (entry.size() > 6 && entry.substr(entry.size() - 6) == ".astra") {
            script.files.push_back(entry);
        }
    }
    std::sort(script.files.begin(), script.files.end());
}

void AstraApp::OpenScriptEditor(std::string directory, std::string preferredFile,
                                std::string repoName) {
    script.fromRepo = !repoName.empty();
    script.repoName = std::move(repoName);
    script.directory = std::move(directory);
    RefreshScriptFiles();

    if (!preferredFile.empty() && fs::Exists(preferredFile)) {
        OpenScriptFile(preferredFile);
    } else if (!script.files.empty()) {
        OpenScriptFile(paths::Join(script.directory, script.files.front()));
    } else if (script.source.empty()) {
        NewScript(script.fromRepo ? "scratch.astra" : "level_tools.astra");
    }
    Goto(Screen::ScriptEditor);
}

void AstraApp::NewScript(std::string fileName) {
    static const char* kTemplate = R"astra(// AstraScript - runs against the open level the moment you press Run.
// The full API lives in docs/SCRIPTING.md: scene, mesh, image, math, io.

print("hello from AstraScript")

let sun = findActor("Directional Light")
if (sun != 0) {
    print("sun actor:", actorName(sun))
}

// Build a ring of pillars with a scripted mesh.
let mesh = meshCylinder(20)
meshRecenter(mesh)
let meshId = meshRegister(mesh, "Script pillar")

for (i in range(8)) {
    let angle = i * TAU / 8.0
    let x = cos(angle) * 6.0
    let z = sin(angle) * 6.0
    let actor = spawnActor(format("Pillar {}", i), "custom", x, 1.5, z)
    attachMesh(actor, meshId)
    setScale(actor, 0.7, 3.0, 0.7)
    setMaterial(actor, "lit", 0.35, 0.1, 0.0)
    setColor(actor, 0.42, 0.55, 0.78)
}
print("pillars:", actorCount())
)astra";
    script.fileName = std::move(fileName);
    script.source = kTemplate;
    script.filePath.clear();
    script.error.clear();
    script.errorLine = 0;
    script.status = "New script - Run to execute it against the level";
    script.dirty = true;
    script.output.clear();
}

bool AstraApp::OpenScriptFile(const std::string& path) {
    std::string text;
    if (!fs::ReadTextFile(path, text)) {
        PushToast(ToastKind::Error, "Could not open script", path);
        return false;
    }
    script.source = std::move(text);
    script.filePath = path;
    script.fileName = fs::FileName(path);
    script.dirty = false;
    script.error.clear();
    script.errorLine = 0;
    script.status = "Opened " + script.fileName;
    return true;
}

bool AstraApp::SaveScript() {
    std::string path = script.filePath;
    if (path.empty()) {
        if (script.directory.empty()) {
            PushToast(ToastKind::Warning, "No folder selected",
                      "Open a project or repository to give the script a home.");
            return false;
        }
        path = paths::Join(script.directory, script.fileName.empty() ? "untitled.astra"
                                                                   : script.fileName);
    }
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) fs::CreateDirectories(path.substr(0, slash));
    if (!fs::WriteTextFileAtomic(path, script.source)) {
        PushToast(ToastKind::Error, "Could not save script", path);
        return false;
    }
    script.filePath = path;
    script.fileName = fs::FileName(path);
    script.dirty = false;
    RefreshScriptFiles();
    SetFooterStatus("Saved " + script.fileName);
    return true;
}

bool AstraApp::RunScript() {
    if (!scriptHost) scriptHost = std::make_unique<script::Host>(editor.scene);
    scriptHost->Reset();
    scriptHost->SetWorkingDirectory(script.directory);
    scriptHost->SetOutputSink(nullptr);
    PushUndo("run " + script.fileName);

    const script::ScriptRun run = scriptHost->Run(script.source, script.fileName);
    script.output = run.output;
    script.error = run.result.error;
    script.errorLine = run.result.line;
    script.lastMilliseconds = run.result.milliseconds;
    script.lastSteps = run.result.steps;

    if (run.result.ok) {
        script.status = str::Format("Ran %s in %.1f ms (%lld steps)", script.fileName.c_str(),
                                    run.result.milliseconds, run.result.steps);
        SetFooterStatus(script.status);
        editor.scene.MarkDirty();
    } else {
        script.status = str::Format("%s:%d  %s", script.fileName.c_str(), run.result.line,
                                    run.result.error.c_str());
        PushToast(ToastKind::Error, "Script error", script.status);
    }
    return run.result.ok;
}

// ---------------------------------------------------------------------------
// Offline ray tracing
// ---------------------------------------------------------------------------
void AstraApp::StartTrace() {
    if (editor.tracing) return;
    editor.tracing = true;
    editor.traceProgress = 0.0f;
    editor.traceError.clear();
    editor.showRenderPanel = true;
}

bool AstraApp::SaveTraceImage(std::string path) {
    if (editor.traceImage.Empty()) return false;
    if (path.empty()) {
        const std::string base = paths::EngineDataDir();
        fs::CreateDirectories(paths::Join(base, "renders"));
        path = paths::Join(base, "renders/" + str::Format("render-%lld.png",
                                                          static_cast<long long>(time::NowUtc())));
    }
    if (!image::WritePng(editor.traceImage, path)) {
        editor.traceError = "Could not write " + path;
        return false;
    }
    editor.tracePath = path;
    SetFooterStatus("Saved render to " + path);
    return true;
}

void AstraApp::ImportFromBlender() {
    const std::vector<asset::BridgeJob> pending = asset::ScanInbox();
    if (pending.empty()) {
        PushToast(ToastKind::Info, "Nothing waiting from Blender",
                  "Use the Astra panel in Blender's sidebar to send a mesh. Folder: " +
                      asset::InboxDirectory());
        return;
    }
    PushUndo("import from Blender");
    const asset::BridgeImportResult result = asset::ImportInbox(editor.scene);
    if (result.imported == 0) {
        PushToast(ToastKind::Error, "Blender import failed",
                  result.errors.empty() ? "The inbox held no usable meshes."
                                        : result.errors.front());
        return;
    }
    if (!result.actorNames.empty()) {
        if (const scene::Entity* actor = editor.scene.FindByName(result.actorNames.back())) {
            editor.selected = actor->id;
        }
    }
    editor.scene.MarkDirty();
    PushToast(ToastKind::Success,
              str::Format("Imported %d mesh(es) from Blender", result.imported),
              str::Format("%d triangles, %d skipped", result.triangles, result.skipped));
    SetFooterStatus(str::Format("Imported %d Blender mesh(es)", result.imported));
}

void AstraApp::RefreshDemoContent() {
    editor.scene = scene::Scene::CreateSampleLevel();
    editor.selected = editor.scene.Roots().empty() ? scene::kInvalidEntity
                                                   : editor.scene.Roots().front();
    SetFooterStatus("Sample scene rebuilt");
}

}  // namespace astra::app
