cmake_minimum_required(VERSION 3.20)

project(AstraStudio
    VERSION 0.1.1
    DESCRIPTION "Astra Engine - general-purpose game engine and studio"
    LANGUAGES CXX)

# The engine is still pre-1.0: the viewport rasterises on the CPU and the asset pipeline
# is not in yet, so the project stays on the 0.x line. Bump the VERSION line by hand only
# for deliberate minor releases; the release workflow in .github/workflows/release.yml
# bumps the patch number.
if(PROJECT_VERSION VERSION_GREATER_EQUAL 1.0.0)
    message(FATAL_ERROR "Astra Studio stays below 1.0.0 until the engine API stabilises "
                        "(see docs/ROADMAP.md). Current VERSION is ${PROJECT_VERSION}.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

option(ASTRA_BUILD_STUDIO "Build the windowed Astra Studio and Astra KeyForge apps" ON)
option(ASTRA_BUILD_TOOLS "Build the headless self-check and UI preview tools" ON)
option(ASTRA_FETCH_GLFW "Download GLFW when no local copy is available" ON)
option(ASTRA_WARNINGS "Enable the studio warning set" ON)
set(ASTRA_GLFW_DIR "" CACHE PATH "Use a local GLFW checkout instead of fetching it")

message(STATUS "Astra Studio ${PROJECT_VERSION} (${CMAKE_BUILD_TYPE})")

# ---------------------------------------------------------------------------
# Vendored Dear ImGui (docking branch, MIT)
# ---------------------------------------------------------------------------
add_library(astra_imgui STATIC
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp)
target_include_directories(astra_imgui PUBLIC extern/imgui)
target_compile_definitions(astra_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# ---------------------------------------------------------------------------
# Engine + UI library (everything that is not a frontend)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ASTRA_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astra/*.cpp")
file(GLOB_RECURSE ASTRA_ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astra/*.h")

add_library(astra_engine STATIC ${ASTRA_ENGINE_SOURCES} ${ASTRA_ENGINE_HEADERS})
target_include_directories(astra_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern")
target_link_libraries(astra_engine PUBLIC astra_imgui)
target_compile_definitions(astra_engine PUBLIC
    ASTRA_VERSION_STRING="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(astra_engine PRIVATE /utf-8 /permissive-)
endif()

if(ASTRA_WARNINGS AND NOT MSVC)
    target_compile_options(astra_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(astra_engine PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(astra_engine PUBLIC bcrypt crypt32 winhttp)
endif()

# JPEG/PNG decoding for branding assets.
target_compile_definitions(astra_engine PRIVATE STBI_NO_STDIO)

# ---------------------------------------------------------------------------
# Windowed frontend (GLFW + OpenGL3 + ImGui backends)
# ---------------------------------------------------------------------------
if(ASTRA_BUILD_STUDIO)
    if(NOT WIN32)
        message(STATUS "Windowed apps are Windows-only in this milestone; building headless tools only")
    endif()

    if(WIN32)
        if(ASTRA_GLFW_DIR)
            add_subdirectory("${ASTRA_GLFW_DIR}" "${CMAKE_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
        else()
            include(FetchContent)
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
            FetchContent_Declare(glfw
                GIT_REPOSITORY https://github.com/glfw/glfw.git
                GIT_TAG 3.4
                GIT_SHALLOW TRUE)
            if(ASTRA_FETCH_GLFW)
                FetchContent_MakeAvailable(glfw)
            endif()
        endif()

        if(TARGET glfw)
            add_library(astra_imgui_backends STATIC
                extern/imgui/backends/imgui_impl_glfw.cpp
                extern/imgui/backends/imgui_impl_opengl3.cpp)
            target_include_directories(astra_imgui_backends PUBLIC extern/imgui extern/imgui/backends)
            target_link_libraries(astra_imgui_backends PUBLIC astra_imgui glfw opengl32)

            add_executable(astra_studio WIN32
                src/studio/main.cpp
                src/platform/GlfwApplication.cpp)
            target_link_libraries(astra_studio PRIVATE astra_engine astra_imgui_backends)
            target_compile_definitions(astra_studio PRIVATE ASTRA_GUI_SUBSYSTEM)

            add_executable(astra_keyforge WIN32
                src/keygen/main.cpp
                src/platform/GlfwApplication.cpp)
            target_link_libraries(astra_keyforge PRIVATE astra_engine astra_imgui_backends shell32)
            target_compile_definitions(astra_keyforge PRIVATE ASTRA_GUI_SUBSYSTEM)

            # Ship-friendly names: the packaging script, docs and shortcuts all use these.
            set_target_properties(astra_studio PROPERTIES OUTPUT_NAME "AstraStudio")
            set_target_properties(astra_keyforge PROPERTIES OUTPUT_NAME "AstraKeyForge")

            foreach(target astra_studio astra_keyforge)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${target}>/assets"
                    COMMENT "Copying Astra assets next to ${target}")
            endforeach()

            install(TARGETS astra_studio astra_keyforge RUNTIME DESTINATION ".")
            install(DIRECTORY assets DESTINATION ".")
            install(FILES README.md LICENSE DESTINATION ".")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Headless tools
# ---------------------------------------------------------------------------
if(ASTRA_BUILD_TOOLS)
    add_executable(astra_selfcheck src/tools/SelfCheck.cpp)
    target_link_libraries(astra_selfcheck PRIVATE astra_engine)

    add_executable(astra_uipreview
        src/tools/UiPreview.cpp
        src/tools/SoftRaster.cpp)
    target_link_libraries(astra_uipreview PRIVATE astra_engine)

    if(ASTRA_WARNINGS AND NOT MSVC)
        target_compile_options(astra_selfcheck PRIVATE -Wall -Wextra)
        target_compile_options(astra_uipreview PRIVATE -Wall -Wextra)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests / CTest wiring
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING AND TARGET astra_selfcheck)
    add_test(NAME astra_selfcheck COMMAND astra_selfcheck)
    if(TARGET astra_uipreview)
        add_test(NAME astra_ui_smoke COMMAND astra_uipreview --smoke)
        set_tests_properties(astra_ui_smoke PROPERTIES
            ENVIRONMENT "ASTRA_ASSETS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets")
    endif()
    set_tests_properties(astra_selfcheck PROPERTIES
        ENVIRONMENT "ASTRA_ASSETS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()
