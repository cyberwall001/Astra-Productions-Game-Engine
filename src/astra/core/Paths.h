// core/Paths.h - resolves the per-user data locations and the engine asset root.
#pragma once

#include <string>
#include <string_view>

namespace astra::paths {

// Root of all Astra state for the current user, e.g.
//   Windows: %LOCALAPPDATA%\Astra Productions
//   Linux:   $XDG_DATA_HOME/astra-productions
//   macOS:   ~/Library/Application Support/Astra Productions
std::string DataRoot();

// Optional override used by tests and portable installs (empty input restores default).
void SetDataRootOverride(std::string directory);

// Directory of the running executable.
std::string ExecutableDir();

// Root folder that holds bundled assets (fonts, branding, themes).
// Resolution order: $ASTRA_ASSETS_DIR, <exe dir>/assets, <cwd>/assets, source tree fallback.
std::string AssetRoot();

// Resolves `relative` under AssetRoot().
std::string Asset(std::string_view relative);

// Creates (if needed) and returns a directory under DataRoot(), e.g. "engine/accounts".
std::string EnsureDataDir(std::string_view relative);

// <DataRoot>/engine - launcher + editor state.
std::string EngineDataDir();
// <DataRoot>/keyforge - owner tool state (access code master key records).
std::string KeyForgeDataDir();

std::string Join(const std::string& a, const std::string& b);

}  // namespace astra::paths
