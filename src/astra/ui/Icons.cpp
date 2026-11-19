#include "astra/ui/Icons.h"

#include <cmath>

namespace astra::ui {
namespace {

constexpr float kPi = 3.14159265358979f;

struct Pen {
    ImDrawList* dl;
    ImVec2 c;
    float s;
    ImU32 col;
    float th;

    ImVec2 P(float x, float y) const { return ImVec2(c.x + (x - 0.5f) * s, c.y + (y - 0.5f) * s); }

    void Line(float x0, float y0, float x1, float y1) const {
        dl->AddLine(P(x0, y0), P(x1, y1), col, th);
    }
    void Poly(const ImVec2* points, int count, bool closed = false) const {
        dl->AddPolyline(points, count, col, closed ? ImDrawFlags_Closed : 0, th);
    }
    void Circle(float x, float y, float r, float fill = 0.0f) const {
        if (fill > 0.0f) {
            dl->AddCircleFilled(P(x, y), r * s, fill >= 1.0f ? col : fill, 24);
        } else {
            dl->AddCircle(P(x, y), r * s, col, 24, th);
        }
    }
    void Rect(float x0, float y0, float x1, float y1, float rounding = 0.08f) const {
        dl->AddRect(P(x0, y0), P(x1, y1), col, rounding * s, 0, th);
    }
    void RectFilled(float x0, float y0, float x1, float y1, float rounding = 0.08f) const {
        dl->AddRectFilled(P(x0, y0), P(x1, y1), col, rounding * s);
    }
    void Arc(float x, float y, float r, float a0, float a1) const {
        dl->PathArcTo(P(x, y), r * s, a0, a1, 20);
        dl->PathStroke(col, 0, th);
    }
    void Dot(float x, float y, float r) const { dl->AddCircleFilled(P(x, y), r * s, col, 12); }
};

}  // namespace

const char* IconName(Icon icon) {
    switch (icon) {
        case Icon::Search: return "search";
        case Icon::Plus: return "plus";
        case Icon::Minus: return "minus";
        case Icon::Gear: return "gear";
        case Icon::Close: return "close";
        case Icon::Check: return "check";
        case Icon::Warning: return "warning";
        case Icon::Info: return "info";
        case Icon::Refresh: return "refresh";
        case Icon::Cloud: return "cloud";
        case Icon::Wind: return "wind";
        case Icon::Gauge: return "gauge";
        case Icon::Camera: return "camera";
        case Icon::Light: return "light";
        case Icon::Tag: return "tag";
        case Icon::Move: return "move";
        case Icon::Layers: return "layers";
        case Icon::Cube: return "cube";
        case Icon::Terminal: return "terminal";
        case Icon::Play: return "play";
        case Icon::Pause: return "pause";
        case Icon::Stop: return "stop";
        case Icon::Branch: return "branch";
        case Icon::User: return "user";
        case Icon::Users: return "users";
        case Icon::Lock: return "lock";
        case Icon::Key: return "key";
        case Icon::Logout: return "logout";
        case Icon::Clock: return "clock";
        case Icon::Star: return "star";
        case Icon::Eye: return "eye";
        case Icon::EyeOff: return "eyeoff";
        case Icon::Dots: return "dots";
        case Icon::ArrowRight: return "arrowright";
        case Icon::Download: return "download";
        case Icon::Copy: return "copy";
        case Icon::Trash: return "trash";
        case Icon::Save: return "save";
        case Icon::Grid: return "grid";
        case Icon::List: return "list";
        case Icon::Sun: return "sun";
        case Icon::Bolt: return "bolt";
        case Icon::Folder: return "folder";
        case Icon::Github: return "github";
        case Icon::Shield: return "shield";
        case Icon::Sparkle: return "sparkle";
        case Icon::Code: return "code";
        case Icon::Undo: return "undo";
        case Icon::Redo: return "redo";
        case Icon::Filter: return "filter";
        case Icon::External: return "external";
        case Icon::Database: return "database";
        case Icon::Mail: return "mail";
        case Icon::Wifi: return "wifi";
        case Icon::WifiOff: return "wifioff";
        case Icon::Sliders: return "sliders";
        case Icon::Viewport: return "viewport";
        case Icon::Axis: return "axis";
        case Icon::Flag: return "flag";
        case Icon::Compass: return "compass";
        default: return "none";
    }
}

void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color, float thickness) {
    if (!dl || icon == Icon::None) return;
    Pen p{dl, center, size, color, thickness};

    switch (icon) {
        case Icon::Search:
            p.Circle(0.44f, 0.44f, 0.26f);
            p.Line(0.64f, 0.64f, 0.86f, 0.86f);
            break;
        case Icon::Plus:
            p.Line(0.5f, 0.18f, 0.5f, 0.82f);
            p.Line(0.18f, 0.5f, 0.82f, 0.5f);
            break;
        case Icon::Minus:
            p.Line(0.18f, 0.5f, 0.82f, 0.5f);
            break;
        case Icon::Gear: {
            p.Circle(0.5f, 0.5f, 0.17f);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * kPi / 4.0f;
                const float c = std::cos(a), s = std::sin(a);
                p.Line(0.5f + c * 0.27f, 0.5f + s * 0.27f, 0.5f + c * 0.44f, 0.5f + s * 0.44f);
            }
            break;
        }
        case Icon::Close:
            p.Line(0.24f, 0.24f, 0.76f, 0.76f);
            p.Line(0.76f, 0.24f, 0.24f, 0.76f);
            break;
        case Icon::Check: {
            const ImVec2 pts[] = {p.P(0.18f, 0.53f), p.P(0.41f, 0.76f), p.P(0.83f, 0.26f)};
            p.Poly(pts, 3);
            break;
        }
        case Icon::Warning: {
            const ImVec2 pts[] = {p.P(0.5f, 0.10f), p.P(0.94f, 0.86f), p.P(0.06f, 0.86f)};
            p.Poly(pts, 3, true);
            p.Line(0.5f, 0.38f, 0.5f, 0.60f);
            p.Dot(0.5f, 0.72f, 0.055f);
            break;
        }
        case Icon::Info:
            p.Circle(0.5f, 0.5f, 0.38f);
            p.Line(0.5f, 0.44f, 0.5f, 0.70f);
            p.Dot(0.5f, 0.32f, 0.055f);
            break;
        case Icon::Refresh:
            p.Arc(0.5f, 0.5f, 0.33f, -2.6f, 1.5f);
            p.Line(0.80f, 0.30f, 0.84f, 0.52f);
            p.Line(0.84f, 0.52f, 0.63f, 0.47f);
            break;
        case Icon::ChevronDown: {
            const ImVec2 pts[] = {p.P(0.24f, 0.40f), p.P(0.5f, 0.66f), p.P(0.76f, 0.40f)};
            p.Poly(pts, 3);
            break;
        }
        case Icon::ChevronUp: {
            const ImVec2 pts[] = {p.P(0.24f, 0.60f), p.P(0.5f, 0.34f), p.P(0.76f, 0.60f)};
            p.Poly(pts, 3);
            break;
        }
        case Icon::ChevronRight: {
            const ImVec2 pts[] = {p.P(0.40f, 0.24f), p.P(0.66f, 0.5f), p.P(0.40f, 0.76f)};
            p.Poly(pts, 3);
            break;
        }
        case Icon::ChevronLeft: {
            const ImVec2 pts[] = {p.P(0.60f, 0.24f), p.P(0.34f, 0.5f), p.P(0.60f, 0.76f)};
            p.Poly(pts, 3);
            break;
        }
        case Icon::Cloud: {
            p.Circle(0.36f, 0.56f, 0.18f);
            p.Circle(0.56f, 0.46f, 0.22f);
            p.Circle(0.72f, 0.58f, 0.16f);
            p.Line(0.28f, 0.74f, 0.74f, 0.74f);
            break;
        }
        case Icon::Wind:
            p.Line(0.14f, 0.36f, 0.68f, 0.36f);
            p.Arc(0.68f, 0.29f, 0.09f, 1.2f, 4.6f);
            p.Line(0.14f, 0.56f, 0.80f, 0.56f);
            p.Arc(0.80f, 0.65f, 0.09f, 4.2f, 7.6f);
            p.Line(0.30f, 0.76f, 0.62f, 0.76f);
            break;
        case Icon::Gauge:
            p.Arc(0.5f, 0.62f, 0.34f, kPi, 2.0f * kPi);
            p.Line(0.5f, 0.62f, 0.68f, 0.38f);
            p.Dot(0.5f, 0.62f, 0.06f);
            break;
        case Icon::Camera:
            p.Rect(0.10f, 0.30f, 0.90f, 0.78f, 0.14f);
            p.Circle(0.5f, 0.54f, 0.16f);
            p.RectFilled(0.30f, 0.20f, 0.52f, 0.30f, 0.06f);
            break;
        case Icon::Light: {
            p.Circle(0.5f, 0.44f, 0.22f);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * kPi / 4.0f;
                const float c = std::cos(a), s = std::sin(a);
                p.Line(0.5f + c * 0.30f, 0.44f + s * 0.30f, 0.5f + c * 0.42f, 0.44f + s * 0.42f);
            }
            break;
        }
        case Icon::Tag: {
            const ImVec2 pts[] = {p.P(0.14f, 0.14f), p.P(0.54f, 0.14f), p.P(0.86f, 0.46f),
                                  p.P(0.50f, 0.86f), p.P(0.14f, 0.50f)};
            p.Poly(pts, 5, true);
            p.Dot(0.32f, 0.32f, 0.06f);
            break;
        }
        case Icon::Move: {
            p.Line(0.5f, 0.12f, 0.5f, 0.88f);
            p.Line(0.12f, 0.5f, 0.88f, 0.5f);
            const ImVec2 a[] = {p.P(0.38f, 0.22f), p.P(0.5f, 0.10f), p.P(0.62f, 0.22f)};
            const ImVec2 b[] = {p.P(0.38f, 0.78f), p.P(0.5f, 0.90f), p.P(0.62f, 0.78f)};
            const ImVec2 c[] = {p.P(0.22f, 0.38f), p.P(0.10f, 0.5f), p.P(0.22f, 0.62f)};
            const ImVec2 d[] = {p.P(0.78f, 0.38f), p.P(0.90f, 0.5f), p.P(0.78f, 0.62f)};
            p.Poly(a, 3);
            p.Poly(b, 3);
            p.Poly(c, 3);
            p.Poly(d, 3);
            break;
        }
        case Icon::Layers: {
            const ImVec2 a[] = {p.P(0.5f, 0.12f), p.P(0.88f, 0.32f), p.P(0.5f, 0.52f), p.P(0.12f, 0.32f)};
            p.Poly(a, 4, true);
            const ImVec2 b[] = {p.P(0.12f, 0.52f), p.P(0.5f, 0.72f), p.P(0.88f, 0.52f)};
            p.Poly(b, 3);
            const ImVec2 c[] = {p.P(0.12f, 0.70f), p.P(0.5f, 0.90f), p.P(0.88f, 0.70f)};
            p.Poly(c, 3);
            break;
        }
        case Icon::Cube: {
            const ImVec2 top[] = {p.P(0.5f, 0.10f), p.P(0.88f, 0.30f), p.P(0.5f, 0.50f), p.P(0.12f, 0.30f)};
            p.Poly(top, 4, true);
            p.Line(0.12f, 0.30f, 0.12f, 0.70f);
            p.Line(0.88f, 0.30f, 0.88f, 0.70f);
            p.Line(0.5f, 0.50f, 0.5f, 0.90f);
            const ImVec2 bottom[] = {p.P(0.12f, 0.70f), p.P(0.5f, 0.90f), p.P(0.88f, 0.70f)};
            p.Poly(bottom, 3);
            break;
        }
        case Icon::Terminal:
            p.Rect(0.08f, 0.16f, 0.92f, 0.84f, 0.12f);
            p.Line(0.24f, 0.38f, 0.40f, 0.50f);
            p.Line(0.40f, 0.50f, 0.24f, 0.62f);
            p.Line(0.48f, 0.64f, 0.68f, 0.64f);
            break;
        case Icon::Play: {
            const ImVec2 pts[] = {p.P(0.28f, 0.16f), p.P(0.84f, 0.50f), p.P(0.28f, 0.84f)};
            dl->AddTriangleFilled(pts[0], pts[1], pts[2], color);
            break;
        }
        case Icon::Pause:
            p.RectFilled(0.26f, 0.18f, 0.42f, 0.82f, 0.06f);
            p.RectFilled(0.58f, 0.18f, 0.74f, 0.82f, 0.06f);
            break;
        case Icon::Stop:
            p.RectFilled(0.24f, 0.24f, 0.76f, 0.76f, 0.10f);
            break;
        case Icon::Branch:
            p.Circle(0.3f, 0.2f, 0.1f);
            p.Circle(0.3f, 0.8f, 0.1f);
            p.Circle(0.72f, 0.4f, 0.1f);
            p.Line(0.3f, 0.3f, 0.3f, 0.7f);
            p.Line(0.3f, 0.5f, 0.5f, 0.5f);
            p.Line(0.5f, 0.5f, 0.62f, 0.44f);
            break;
        case Icon::User:
            p.Circle(0.5f, 0.32f, 0.18f);
            p.Arc(0.5f, 0.92f, 0.32f, kPi, 2.0f * kPi);
            break;
        case Icon::Users:
            p.Circle(0.36f, 0.34f, 0.16f);
            p.Arc(0.36f, 0.92f, 0.28f, kPi, 2.0f * kPi);
            p.Circle(0.72f, 0.38f, 0.13f);
            p.Arc(0.74f, 0.86f, 0.22f, kPi * 1.15f, kPi * 1.95f);
            break;
        case Icon::Lock:
            p.Rect(0.18f, 0.44f, 0.82f, 0.88f, 0.14f);
            p.Arc(0.5f, 0.44f, 0.18f, kPi, 2.0f * kPi);
            p.Line(0.5f, 0.60f, 0.5f, 0.72f);
            break;
        case Icon::Key:
            p.Circle(0.32f, 0.32f, 0.16f);
            p.Line(0.43f, 0.43f, 0.84f, 0.84f);
            p.Line(0.70f, 0.70f, 0.80f, 0.60f);
            p.Line(0.60f, 0.60f, 0.68f, 0.52f);
            break;
        case Icon::Logout:
            p.Line(0.56f, 0.18f, 0.18f, 0.18f);
            p.Line(0.18f, 0.18f, 0.18f, 0.82f);
            p.Line(0.18f, 0.82f, 0.56f, 0.82f);
            p.Line(0.44f, 0.5f, 0.86f, 0.5f);
            p.Line(0.72f, 0.36f, 0.86f, 0.5f);
            p.Line(0.72f, 0.64f, 0.86f, 0.5f);
            break;
        case Icon::Clock:
            p.Circle(0.5f, 0.5f, 0.38f);
            p.Line(0.5f, 0.28f, 0.5f, 0.52f);
            p.Line(0.5f, 0.52f, 0.66f, 0.62f);
            break;
        case Icon::Star: {
            ImVec2 pts[10];
            for (int i = 0; i < 10; ++i) {
                const float a = -kPi / 2.0f + static_cast<float>(i) * kPi / 5.0f;
                const float r = (i % 2 == 0) ? 0.42f : 0.18f;
                pts[i] = p.P(0.5f + std::cos(a) * r, 0.5f + std::sin(a) * r);
            }
            dl->AddConvexPolyFilled(pts, 10, color);
            break;
        }
        case Icon::Eye:
            p.Arc(0.5f, 0.86f, 0.44f, kPi * 1.22f, kPi * 1.78f);
            p.Arc(0.5f, 0.14f, 0.44f, kPi * 0.22f, kPi * 0.78f);
            p.Circle(0.5f, 0.5f, 0.14f);
            break;
        case Icon::EyeOff:
            p.Arc(0.5f, 0.86f, 0.44f, kPi * 1.22f, kPi * 1.78f);
            p.Line(0.18f, 0.82f, 0.82f, 0.18f);
            break;
        case Icon::Dots:
            p.Dot(0.24f, 0.5f, 0.07f);
            p.Dot(0.5f, 0.5f, 0.07f);
            p.Dot(0.76f, 0.5f, 0.07f);
            break;
        case Icon::ArrowRight:
            p.Line(0.16f, 0.5f, 0.82f, 0.5f);
            p.Line(0.60f, 0.28f, 0.84f, 0.5f);
            p.Line(0.60f, 0.72f, 0.84f, 0.5f);
            break;
        case Icon::Download:
            p.Line(0.5f, 0.14f, 0.5f, 0.64f);
            p.Line(0.30f, 0.46f, 0.5f, 0.66f);
            p.Line(0.70f, 0.46f, 0.5f, 0.66f);
            p.Line(0.16f, 0.80f, 0.84f, 0.80f);
            break;
        case Icon::Copy:
            p.Rect(0.30f, 0.28f, 0.84f, 0.84f, 0.12f);
            p.Line(0.68f, 0.28f, 0.68f, 0.20f);
            p.Line(0.68f, 0.20f, 0.16f, 0.20f);
            p.Line(0.16f, 0.20f, 0.16f, 0.70f);
            p.Line(0.16f, 0.70f, 0.30f, 0.70f);
            break;
        case Icon::Trash:
            p.Line(0.16f, 0.26f, 0.84f, 0.26f);
            p.Line(0.38f, 0.26f, 0.40f, 0.14f);
            p.Line(0.60f, 0.26f, 0.62f, 0.14f);
            p.Line(0.40f, 0.14f, 0.62f, 0.14f);
            p.Line(0.24f, 0.26f, 0.30f, 0.86f);
            p.Line(0.76f, 0.26f, 0.70f, 0.86f);
            p.Line(0.30f, 0.86f, 0.70f, 0.86f);
            break;
        case Icon::Save:
            p.Rect(0.14f, 0.14f, 0.86f, 0.86f, 0.12f);
            p.RectFilled(0.32f, 0.14f, 0.68f, 0.38f, 0.04f);
            p.Rect(0.30f, 0.56f, 0.70f, 0.86f, 0.06f);
            break;
        case Icon::Grid: {
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    const float x = 0.14f + static_cast<float>(col) * 0.40f;
                    const float y = 0.14f + static_cast<float>(row) * 0.40f;
                    p.Rect(x, y, x + 0.32f, y + 0.32f, 0.10f);
                }
            }
            break;
        }
        case Icon::List:
            for (int i = 0; i < 3; ++i) {
                const float y = 0.24f + static_cast<float>(i) * 0.26f;
                p.Dot(0.20f, y, 0.055f);
                p.Line(0.36f, y, 0.84f, y);
            }
            break;
        case Icon::Sun:
            p.Circle(0.5f, 0.5f, 0.18f);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * kPi / 4.0f;
                const float c = std::cos(a), s = std::sin(a);
                p.Line(0.5f + c * 0.28f, 0.5f + s * 0.28f, 0.5f + c * 0.42f, 0.5f + s * 0.42f);
            }
            break;
        case Icon::Bolt: {
            const ImVec2 pts[] = {p.P(0.58f, 0.08f), p.P(0.26f, 0.56f), p.P(0.48f, 0.56f),
                                  p.P(0.40f, 0.92f), p.P(0.74f, 0.42f), p.P(0.52f, 0.42f)};
            dl->AddConvexPolyFilled(pts, 6, color);
            break;
        }
        case Icon::Folder:
            p.Line(0.10f, 0.80f, 0.10f, 0.24f);
            p.Line(0.10f, 0.24f, 0.40f, 0.24f);
            p.Line(0.40f, 0.24f, 0.47f, 0.34f);
            p.Line(0.47f, 0.34f, 0.88f, 0.34f);
            p.Line(0.88f, 0.34f, 0.88f, 0.80f);
            p.Line(0.88f, 0.80f, 0.10f, 0.80f);
            break;
        case Icon::Github: {
            // Simplified mark: circle body with a tail, matching GitHub's silhouette.
            p.Circle(0.5f, 0.46f, 0.34f, 1.0f);
            p.RectFilled(0.40f, 0.72f, 0.60f, 0.90f, 0.10f);
            p.Dot(0.38f, 0.44f, 0.05f);
            p.Dot(0.62f, 0.44f, 0.05f);
            break;
        }
        case Icon::Shield: {
            const ImVec2 pts[] = {p.P(0.5f, 0.10f), p.P(0.86f, 0.26f), p.P(0.80f, 0.62f),
                                  p.P(0.5f, 0.90f), p.P(0.20f, 0.62f), p.P(0.14f, 0.26f)};
            p.Poly(pts, 6, true);
            break;
        }
        case Icon::Sparkle: {
            const ImVec2 pts[] = {p.P(0.5f, 0.08f), p.P(0.60f, 0.40f), p.P(0.92f, 0.5f),
                                  p.P(0.60f, 0.60f), p.P(0.5f, 0.92f), p.P(0.40f, 0.60f),
                                  p.P(0.08f, 0.5f), p.P(0.40f, 0.40f)};
            dl->AddConvexPolyFilled(pts, 8, color);
            break;
        }
        case Icon::Code:
            p.Line(0.36f, 0.24f, 0.14f, 0.5f);
            p.Line(0.14f, 0.5f, 0.36f, 0.76f);
            p.Line(0.64f, 0.24f, 0.86f, 0.5f);
            p.Line(0.86f, 0.5f, 0.64f, 0.76f);
            break;
        case Icon::Undo:
            p.Arc(0.56f, 0.56f, 0.28f, kPi * 0.9f, kPi * 2.1f);
            p.Line(0.28f, 0.44f, 0.28f, 0.68f);
            p.Line(0.28f, 0.68f, 0.50f, 0.68f);
            break;
        case Icon::Redo:
            p.Arc(0.44f, 0.56f, 0.28f, kPi * 1.0f, kPi * 2.2f);
            p.Line(0.72f, 0.44f, 0.72f, 0.68f);
            p.Line(0.72f, 0.68f, 0.50f, 0.68f);
            break;
        case Icon::Filter: {
            const ImVec2 pts[] = {p.P(0.12f, 0.20f), p.P(0.88f, 0.20f), p.P(0.58f, 0.54f),
                                  p.P(0.58f, 0.86f), p.P(0.42f, 0.78f), p.P(0.42f, 0.54f)};
            p.Poly(pts, 6, true);
            break;
        }
        case Icon::External:
            p.Line(0.24f, 0.24f, 0.24f, 0.80f);
            p.Line(0.24f, 0.80f, 0.80f, 0.80f);
            p.Line(0.48f, 0.52f, 0.84f, 0.16f);
            p.Line(0.60f, 0.16f, 0.84f, 0.16f);
            p.Line(0.84f, 0.16f, 0.84f, 0.40f);
            break;
        case Icon::Database:
            p.Arc(0.5f, 0.24f, 0.32f, 0.0f, kPi);
            p.Line(0.18f, 0.24f, 0.18f, 0.74f);
            p.Line(0.82f, 0.24f, 0.82f, 0.74f);
            p.Arc(0.5f, 0.74f, 0.32f, 0.0f, kPi);
            p.Arc(0.5f, 0.50f, 0.32f, 0.0f, kPi);
            break;
        case Icon::Mail:
            p.Rect(0.10f, 0.24f, 0.90f, 0.76f, 0.10f);
            p.Line(0.10f, 0.28f, 0.5f, 0.56f);
            p.Line(0.90f, 0.28f, 0.5f, 0.56f);
            break;
        case Icon::Wifi:
            p.Arc(0.5f, 0.78f, 0.40f, kPi * 1.22f, kPi * 1.78f);
            p.Arc(0.5f, 0.78f, 0.26f, kPi * 1.22f, kPi * 1.78f);
            p.Dot(0.5f, 0.74f, 0.07f);
            break;
        case Icon::WifiOff:
            p.Arc(0.5f, 0.78f, 0.34f, kPi * 1.24f, kPi * 1.76f);
            p.Line(0.16f, 0.84f, 0.84f, 0.16f);
            break;
        case Icon::Sliders:
            p.Line(0.14f, 0.3f, 0.86f, 0.3f);
            p.Line(0.14f, 0.7f, 0.86f, 0.7f);
            p.Dot(0.36f, 0.3f, 0.10f);
            p.Dot(0.64f, 0.7f, 0.10f);
            break;
        case Icon::Viewport:
            p.Rect(0.08f, 0.20f, 0.92f, 0.80f, 0.10f);
            p.Line(0.08f, 0.34f, 0.92f, 0.34f);
            p.Dot(0.18f, 0.27f, 0.045f);
            p.Dot(0.28f, 0.27f, 0.045f);
            break;
        case Icon::Axis:
            p.Line(0.2f, 0.8f, 0.8f, 0.8f);
            p.Line(0.2f, 0.8f, 0.2f, 0.2f);
            p.Line(0.2f, 0.8f, 0.62f, 0.44f);
            break;
        case Icon::Flag:
            p.Line(0.28f, 0.10f, 0.28f, 0.90f);
            p.Line(0.28f, 0.14f, 0.82f, 0.26f);
            p.Line(0.82f, 0.26f, 0.28f, 0.48f);
            break;
        case Icon::Compass:
            p.Circle(0.5f, 0.5f, 0.38f);
            p.Line(0.66f, 0.34f, 0.56f, 0.56f);
            p.Line(0.56f, 0.56f, 0.34f, 0.66f);
            p.Line(0.34f, 0.66f, 0.44f, 0.44f);
            p.Line(0.44f, 0.44f, 0.66f, 0.34f);
            break;
        default: break;
    }
}

}  // namespace astra::ui
