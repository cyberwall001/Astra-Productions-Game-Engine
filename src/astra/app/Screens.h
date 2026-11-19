// app/Screens.h - the individual full-screen views.
#pragma once

#include "astra/app/AstraApp.h"

namespace astra::app {

namespace boot {
struct Stage {
    const char* label;
    const char* detail;
    float at;  // seconds after launch
};
const std::vector<Stage>& Stages();
const std::vector<std::string>& TerminalLines();
float LoadingDuration();
float TotalDuration();
}  // namespace boot

namespace screens {
void Boot(AstraApp& app);
void SignIn(AstraApp& app);
void RegisterCode(AstraApp& app);
void RegisterAccount(AstraApp& app);
void ProjectHub(AstraApp& app);
void Editor(AstraApp& app);
void Overlays(AstraApp& app);
void SettingsModal(AstraApp& app);
}  // namespace screens

// Shared chrome.
void DrawLauncherTopBar(AstraApp& app);  // used by the project hub
void DrawToastHost(AstraApp& app);

}  // namespace astra::app
