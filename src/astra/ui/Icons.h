// ui/Icons.h - a small vector icon set drawn straight into ImGui draw lists.
// Keeps Astra free of icon fonts (and their licensing), and scales crisply.
#pragma once

#include "imgui.h"

namespace astra::ui {

enum class Icon {
    None = 0,
    Search, Plus, Minus, Gear, Close, Check, Warning, Info, Refresh,
    ChevronDown, ChevronRight, ChevronLeft, ChevronUp,
    Cloud, Wind, Gauge, Camera, Light, Tag, Move, Layers, Cube,
    Terminal, Play, Pause, Stop, Branch, User, Users, Lock, Key, Logout,
    Clock, Star, Eye, EyeOff, Dots, ArrowRight, Download, Copy, Trash,
    Save, Grid, List, Sun, Bolt, Folder, Github, Shield, Sparkle, Code,
    Undo, Redo, Filter, External, Database, Mail, Wifi, WifiOff, Sliders,
    Viewport, Axis, Flag, Compass,
};

// Draws `icon` centred at `center` inside a square of `size` pixels.
void DrawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color,
              float thickness = 1.7f);

const char* IconName(Icon icon);

}  // namespace astra::ui
