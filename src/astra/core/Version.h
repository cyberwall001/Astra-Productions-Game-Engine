// core/Version.h - single source of truth for product/version strings.
#pragma once

#include <string>

namespace astra {

inline constexpr const char* kStudioName = "Astra Studio";
inline constexpr const char* kEngineName = "Astra Engine";
inline constexpr const char* kKeyForgeName = "Astra KeyForge";
inline constexpr const char* kCompany = "Astra Productions";
// CMake injects the project VERSION here so the UI can never drift from the shipped tag.
#ifdef ASTRA_VERSION_STRING
inline constexpr const char* kEngineVersion = ASTRA_VERSION_STRING;
#else
inline constexpr const char* kEngineVersion = "0.1.2";
#endif
inline constexpr const char* kEngineCodename = "Cumulonimbus";
inline constexpr const char* kBuildChannel = "dev";
inline constexpr int kAccessCodeVersion = 1;
inline constexpr int kAccountSchemaVersion = 1;
inline constexpr int kSceneSchemaVersion = 1;

// e.g. "0.1.0-dev (Cumulonimbus)"
std::string FullVersionString();

}  // namespace astra
