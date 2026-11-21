// app/AstraApp.h - the launcher + editor application object.
//
// One object owns every piece of state the UI needs. Both the windowed frontend
// (GLFW) and the headless preview/self-check tools drive the exact same code.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "astra/app/Settings.h"
#include "astra/auth/AuthService.h"
#include "astra/github/GitHubClient.h"
#include "astra/licensing/AccessCode.h"
#include "astra/game/Player.h"
#include "astra/image/Image.h"
#include "astra/project/Project.h"
#include "astra/render/Camera.h"
#include "astra/render/Pathtracer.h"
#include "astra/render/ViewportTypes.h"
#include "astra/scene/Scene.h"
#include "astra/script/Host.h"

namespace astra::app {

enum class Screen { Boot, SignIn, RegisterCode, RegisterAccount, ProjectHub, Editor, ScriptEditor };

enum class ToastKind { Info, Success, Warning, Error };

struct Toast {
    ToastKind kind = ToastKind::Info;
    std::string title;
    std::string body;
    float age = 0.0f;
    float ttl = 5.0f;
};

struct SignInForm {
    std::string identity;
    std::string password;
    bool remember = true;
    bool reveal = false;
    std::string error;
    std::string hint;
    int attempts = 0;
    bool submitting = false;
};

struct RegisterForm {
    std::string code;
    license::CodeResult codeResult;
    bool codeAccepted = false;

    std::string displayName;
    std::string username;
    std::string email;
    std::string password;
    std::string confirmPassword;
    bool revealPassword = false;
    bool acceptedTerms = false;
    std::string error;
    std::string hint;

    void Reset() { *this = RegisterForm{}; }
};

struct HubState {
    std::string search;
    int filter = 0;      // 0 all, 1 astra projects, 2 private, 3 archived
    int sortMode = 0;    // 0 recently updated, 1 name, 2 stars
    bool gridView = true;
    bool showTokenPanel = false;
    std::string tokenInput;
    std::string tokenError;
    bool connecting = false;
    std::string hoveredRepo;
    std::string newProjectName;
};

// One snapshot-based undo entry: the whole level as JSON plus a label. Levels are small
// (a few KB), so snapshotting is simpler and far more robust than per-field commands.
struct UndoEntry {
    std::string label;
    std::string sceneJson;
};

// The script editor is a full screen (Blender's Text Editor / Unreal's IDE hand-off): a file
// list, a code buffer, a run button and the console the script writes to.
struct ScriptState {
    std::string directory;      // base folder shown in the browser
    std::string filePath;       // absolute path of the open file ("" = scratch buffer)
    std::string fileName = "untitled.astra";
    std::string source;
    std::vector<std::string> files;
    std::vector<std::string> output;
    std::string error;
    int errorLine = 0;
    std::string status;
    bool dirty = false;
    bool fromRepo = false;
    std::string repoName;
    double lastMilliseconds = 0.0;
    long long lastSteps = 0;
    int templateIndex = 0;
    int imageIndex = 0;
};

struct EditorState {
    scene::Scene scene;
    project::ProjectInfo project;
    scene::EntityId selected = scene::kInvalidEntity;
    int tool = 1;  // 0 select, 1 move, 2 rotate, 3 scale
    int transformSpace = 0;  // 0 world, 1 local
    bool snapToGrid = true;
    bool playing = false;
    bool simulating = false;

    // Level viewport.
    render::Camera viewportCamera;
    render::Options viewport;
    render::Stats viewportStats;
    bool viewportFocus = false;   // the mouse is over the viewport this frame
    // Level viewport rect in screen space, published by the panel so tools and tests can
    // pick with exactly the projection that was drawn.
    float viewportMinX = 0.0f;
    float viewportMinY = 0.0f;
    float viewportMaxX = 0.0f;
    float viewportMaxY = 0.0f;
    int bottomTab = 0;            // 0 Content Browser, 1 Output Log
    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showContentBrowser = true;
    bool showStatistics = true;
    bool showWeatherGauge = true;
    bool showWorldSettings = false;
    // ---- editing history -------------------------------------------------
    std::vector<UndoEntry> undo;
    std::vector<UndoEntry> redo;
    int undoLimit = 64;
    std::string pendingUndoLabel;

    // ---- play in editor --------------------------------------------------
    game::Player player;
    bool playerSpawned = false;
    float playTime = 0.0f;
    bool playPossessCamera = true;

    // ---- offline ray tracing ("Render" panel) ----------------------------
    render::TraceSettings trace;
    render::TraceStats traceStats;
    image::Buffer traceImage;
    std::string tracePath;
    std::string traceError;
    float traceProgress = 0.0f;
    bool tracing = false;
    bool showRenderPanel = false;
    int materialPreset = 0;
    int particlePresetIndex = 0;

    int consoleFilter = 0;  // 0 all, 1 info, 2 warn, 3 error
    std::string consoleInput;
    std::vector<std::string> consoleHistory;
    std::string hierarchySearch;
    std::string contentPath = "Content";
    int contentFilter = 0;
    std::vector<float> frameHistory;
    float autosaveTimer = 0.0f;
    bool autosave = true;
    bool layoutInitialized = false;
    float renameTimer = 0.0f;
    std::string statusText;
    float statusTimer = 0.0f;
};

struct AstraApp {
    // ---- services -------------------------------------------------------
    SettingsStore settings;
    auth::AuthService auth;
    project::RecentProjects recents;
    github::RepoFetcher fetcher;
    github::RepoSnapshot repos;

    // ---- lifecycle ------------------------------------------------------
    Screen screen = Screen::Boot;
    Screen pendingScreen = Screen::Boot;
    float transition = 0.0f;   // 0..1 fade-in for the current screen
    float time = 0.0f;         // seconds since launch
    float dt = 1.0f / 60.0f;
    double launchMs = 0.0;
    bool headless = false;
    bool wantsQuit = false;
    bool initialized = false;

    // ---- boot -----------------------------------------------------------
    float bootTimer = 0.0f;
    int bootStage = 0;
    int bootStageCount = 0;
    bool bootLogTyped = false;
    bool autoSignInChecked = false;

    // ---- forms / panels -------------------------------------------------
    SignInForm signIn;
    RegisterForm reg;
    HubState hub;
    EditorState editor;
    ScriptState script;
    std::unique_ptr<script::Host> scriptHost;
    std::vector<Toast> toasts;

    bool showSettings = false;
    int settingsTab = 0;
    bool guestMode = false;
    bool showCommandPalette = false;
    std::string commandText;
    std::string footerStatus;
    float footerStatusTimer = 0.0f;

    // ---- api ------------------------------------------------------------
    void Init(bool headlessMode);
    void Tick(float deltaTime);
    void Draw();
    void Shutdown();

    void Goto(Screen next, bool instant = false);
    void PushToast(ToastKind kind, std::string title, std::string body = {});
    void SetFooterStatus(std::string text);

    void SignInUser(const auth::Account& account);
    void SignOut();
    void SubmitSignIn();
    void SubmitCode();
    void SubmitRegistration();

    void StartRepoSync(bool forceDemo = false);
    void OpenRepo(const github::Repo& repo);
    void OpenProject(const project::ProjectInfo& info);
    void CreateProject(const std::string& name);
    void CloseProject();
    void SaveScene();
    void NewScene();
    void RefreshDemoContent();

    // ---- editor commands -------------------------------------------------
    void PushUndo(std::string label);
    void Undo();
    void Redo();
    bool CanUndo() const { return !editor.undo.empty(); }
    bool CanRedo() const { return !editor.redo.empty(); }
    void DuplicateSelected();
    void DeleteSelected();

    // ---- play in editor --------------------------------------------------
    void EnterPlay();
    void ExitPlay();

    // ---- scripting -------------------------------------------------------
    void OpenScriptEditor(std::string directory, std::string preferredFile = {},
                          std::string repoName = {});
    void RefreshScriptFiles();
    bool OpenScriptFile(const std::string& path);
    bool SaveScript();
    bool RunScript();
    void NewScript(std::string fileName = "untitled.astra");

    // ---- offline rendering ----------------------------------------------
    void StartTrace();
    bool SaveTraceImage(std::string path = {});

    std::string GitHubToken() const;
    void SetGitHubToken(const std::string& token, const std::string& login);

    const auth::Account* CurrentUser() const { return auth.Current(); }
    bool IsEditorActive() const { return screen == Screen::Editor; }
};

// Draws the whole app (root window + current screen + overlays).
void DrawAstraApp(AstraApp& app);

}  // namespace astra::app
