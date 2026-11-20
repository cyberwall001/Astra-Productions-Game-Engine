#include "astra/core/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "astra/core/FileIO.h"

namespace stdfs = std::filesystem;
using namespace astra;

namespace astra::paths {
namespace {

std::string gDataRootOverride;

std::string EnvironmentVariable(const char* name) {
#if defined(_WIN32)
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) == 0 && buffer) {
        std::string value(buffer);
        std::free(buffer);
        return value;
    }
    return {};
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

std::string HomeDir() {
#if defined(_WIN32)
    std::string profile = EnvironmentVariable("USERPROFILE");
    if (!profile.empty()) return profile;
    return EnvironmentVariable("HOMEDRIVE") + EnvironmentVariable("HOMEPATH");
#else
    const std::string home = EnvironmentVariable("HOME");
    return home.empty() ? std::string(".") : home;
#endif
}

}  // namespace

std::string Join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return (stdfs::u8path(a) / stdfs::u8path(b)).u8string();
}

std::string DataRoot() {
    if (!gDataRootOverride.empty()) return gDataRootOverride;
#if defined(_WIN32)
    std::string local = EnvironmentVariable("LOCALAPPDATA");
    if (local.empty()) local = Join(HomeDir(), "AppData/Local");
    return Join(local, "Astra Productions");
#elif defined(__APPLE__)
    return Join(Join(HomeDir(), "Library/Application Support"), "Astra Productions");
#else
    std::string xdg = EnvironmentVariable("XDG_DATA_HOME");
    if (xdg.empty()) xdg = Join(HomeDir(), ".local/share");
    return Join(xdg, "astra-productions");
#endif
}

void SetDataRootOverride(std::string directory) { gDataRootOverride = std::move(directory); }

std::string ExecutableDir() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buffer[4096];
    const unsigned long len = GetModuleFileNameW(nullptr, buffer, 4096);
    if (len > 0) {
        // Build the path from the wide string directly: u8path's wide-char factory is
        // deprecated ([depr.fs.path.factory]/1) and MSVC's STL rejects it outright.
        const stdfs::path exe(std::wstring_view(buffer, static_cast<std::size_t>(len)));
        return exe.parent_path().u8string();
    }
    return {};
#else
    const auto exe = stdfs::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path().u8string();
    return stdfs::current_path(ec).u8string();
#endif
}

std::string AssetRoot() {
    const std::string fromEnv = EnvironmentVariable("ASTRA_ASSETS_DIR");
    if (!fromEnv.empty() && fs::IsDirectory(fromEnv)) return fromEnv;

    const std::string exeAssets = Join(ExecutableDir(), "assets");
    if (fs::IsDirectory(exeAssets)) return exeAssets;

    std::error_code ec;
    const std::string cwdAssets = Join(stdfs::current_path(ec).u8string(), "assets");
    if (fs::IsDirectory(cwdAssets)) return cwdAssets;

    return cwdAssets;
}

std::string Asset(std::string_view relative) { return Join(AssetRoot(), std::string(relative)); }

std::string EnsureDataDir(std::string_view relative) {
    const std::string dir = Join(DataRoot(), std::string(relative));
    fs::CreateDirectories(dir);
    return dir;
}

std::string EngineDataDir() { return EnsureDataDir("engine"); }

std::string KeyForgeDataDir() { return EnsureDataDir("keyforge"); }

}  // namespace astra::paths
