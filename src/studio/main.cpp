// src/studio/main.cpp - Astra Studio (launcher + editor) entry point.
#include "astra/platform/Application.h"

#if defined(_WIN32) && defined(ASTRA_GUI_SUBSYSTEM)
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return astra::platform::RunStudioWindow();
}
#else
int main(int, char**) { return astra::platform::RunStudioWindow(); }
#endif
