// platform/Application.h - windowed entry points.
//
// The window + GL init lives behind this interface so the launcher, editor and
// KeyForge share one frontend, and so headless tools never pull in GLFW.
#pragma once

namespace astra::platform {

// Opens the Astra Studio window (launcher -> project hub -> editor).
// Returns the process exit code.
int RunStudioWindow();

// Opens the Astra KeyForge window (owner-only access code issuer).
int RunKeyForgeWindow();

}  // namespace astra::platform
