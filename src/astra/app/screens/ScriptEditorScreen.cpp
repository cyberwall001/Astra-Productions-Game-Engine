// app/screens/ScriptEditorScreen.cpp - the AstraScript workspace.
//
// Deliberately Blender-shaped: a file column on the left, a big monospace editor in the
// middle with a line-number gutter, and a docked output/image column on the right. It is the
// screen you land on when a repository has no astra.project.json - script work needs no
// level, so a plain code repo still opens somewhere useful.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "astra/app/Screens.h"
#include "astra/app/screens/Common.h"
#include "astra/core/FileIO.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/image/Image.h"
#include "astra/ui/Theme.h"
#include "astra/ui/Widgets.h"
#include "imgui.h"

namespace astra::app::screens {
namespace {

using ui::Colors;
using ui::Text;
using ui::U;

constexpr float kTopBar = 46.0f;
constexpr float kStatusBar = 26.0f;
constexpr float kGap = 8.0f;

bool IsKeyword(const std::string& word) {
    static const char* kWords[] = {"let",  "var",    "fn",     "return", "if",     "else",
                                   "while", "for",   "in",     "break",  "continue", "true",
                                   "false", "nil"};
    for (const char* candidate : kWords) {
        if (word == candidate) return true;
    }
    return false;
}

struct CodeRun {
    std::string text;
    ImU32 color;
};

// Lexes one source line into coloured runs. Deliberately tiny: keywords, numbers, strings and
// comments are enough to make AstraScript readable without a real parser in the draw path.
void TokenizeLine(const std::string& line, std::vector<CodeRun>& runs) {
    const ImU32 text = Colors().text;
    const ImU32 keyword = Colors().brand;
    const ImU32 number = IM_COL32(240, 180, 94, 255);
    const ImU32 string = IM_COL32(140, 210, 160, 255);
    const ImU32 comment = Colors().textFaint;

    const std::size_t commentAt = line.find("//");
    const std::size_t end = commentAt == std::string::npos ? line.size() : commentAt;

    std::string token;
    const auto flush = [&](ImU32 color) {
        if (token.empty()) return;
        if (!runs.empty() && runs.back().color == color) {
            runs.back().text += token;
        } else {
            runs.push_back({token, color});
        }
        token.clear();
    };

    for (std::size_t i = 0; i < end; ++i) {
        const char c = line[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') {
            token.push_back(c);
            continue;
        }
        if (!token.empty()) {
            const bool numeric = std::isdigit(static_cast<unsigned char>(token[0])) != 0;
            flush(numeric ? number : (IsKeyword(token) ? keyword : text));
        }
        if (c == '"' || c == '\'') {
            std::size_t close = i + 1;
            while (close < end && line[close] != c) ++close;
            token = line.substr(i, std::min(end, close + 1) - i);
            flush(string);
            i = close;
            continue;
        }
        token.push_back(c);
        flush(text);
    }
    if (!token.empty()) {
        const bool numeric = std::isdigit(static_cast<unsigned char>(token[0])) != 0;
        flush(numeric ? number : (IsKeyword(token) ? keyword : text));
    }
    if (commentAt != std::string::npos) {
        token = line.substr(commentAt);
        flush(comment);
    }
}

// Draws one line, wrapping at `wrapWidth` with ImGui's own word-wrap algorithm so the highlight
// lines up with the text input that sits on top of it. Returns the number of visual rows used.
int DrawHighlightedLine(ImDrawList* dl, ImFont* font, ImVec2 position, const std::string& line,
                        float wrapWidth, float lineHeight) {
    if (wrapWidth < U(24.0f)) return 1;
    std::vector<CodeRun> runs;
    TokenizeLine(line, runs);
    if (runs.empty()) return 1;

    float x = position.x;
    int rows = 1;
    for (const CodeRun& run : runs) {
        const char* cursor = run.text.c_str();
        const char* stop = cursor + run.text.size();
        while (cursor < stop) {
            const float available = wrapWidth - (x - position.x);
            const float rowY = position.y + (static_cast<float>(rows) - 1.0f) * lineHeight;
            if (available < font->FontSize) {
                x = position.x;
                ++rows;
                continue;
            }
            const char* lineEnd = font->CalcWordWrapPositionA(
                1.0f, cursor, stop, std::max(available, font->FontSize));
            if (lineEnd <= cursor) lineEnd = stop;
            const std::string chunk(cursor, lineEnd);
            dl->AddText(font, font->FontSize, ImVec2(x, rowY), run.color, chunk.c_str());
            x += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, chunk.c_str()).x;
            cursor = lineEnd;
            const bool wrapped = cursor < stop;
            if (wrapped) {
                x = position.x;
                ++rows;
            }
        }
    }
    return rows;
}

void DrawFileColumn(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().editorPanel, U(8.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(8.0f), 0, U(1.0f));

    const ImVec2 header(pos.x, pos.y);
    dl->AddRectFilled(header, ImVec2(pos.x + size.x, pos.y + U(32.0f)), Colors().editorHeader,
                      U(8.0f), ImDrawFlags_RoundCornersTop);
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(12.0f), pos.y + U(9.0f)),
                Colors().textDim, "Scripts");

    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(8.0f), pos.y + U(40.0f)));
    ImGui::BeginChild("##script_files", ImVec2(size.x - U(16.0f), size.y - U(88.0f)), false,
                      ImGuiWindowFlags_NoBackground);
    if (app.script.files.empty()) {
        ui::TextFaint("No .astra files yet");
    }
    for (const auto& file : app.script.files) {
        const bool active = !app.script.filePath.empty() &&
                            fs::FileName(app.script.filePath) == fs::FileName(file);
        if (ImGui::Selectable(file.c_str(), active, 0, ImVec2(0.0f, U(22.0f)))) {
            app.OpenScriptFile(paths::Join(app.script.directory, file));
        }
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(8.0f), pos.y + size.y - U(40.0f)));
    if (ui::SecondaryButton("New", ImVec2(size.x - U(16.0f), U(30.0f)), true, ui::Icon::Plus)) {
        app.NewScript();
    }
}

void DrawOutputColumn(AstraApp& app, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().editorPanel, U(8.0f));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Colors().stroke, U(8.0f), 0, U(1.0f));

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + U(32.0f)), Colors().editorHeader, U(8.0f),
                      ImDrawFlags_RoundCornersTop);
    dl->AddText(Text().small, Text().small->FontSize, ImVec2(pos.x + U(12.0f), pos.y + U(9.0f)),
                Colors().textDim, "Console");

    ImGui::SetCursorScreenPos(ImVec2(pos.x + U(10.0f), pos.y + U(40.0f)));
    ImGui::BeginChild("##script_output", ImVec2(size.x - U(20.0f), size.y - U(150.0f)), false,
                      ImGuiWindowFlags_NoBackground);
    ui::PushFont(Text().monoSmall);
    for (const auto& line : app.script.output) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().textDim));
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (!app.script.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::ToVec4(Colors().danger));
        ImGui::TextWrapped("%s (line %d)", app.script.error.c_str(), app.script.errorLine);
        ImGui::PopStyleColor();
    }
    if (app.script.output.empty() && app.script.error.empty()) {
        ui::TextFaint("Run a script to see its output");
    }
    ui::PopFont();
    ImGui::EndChild();

    // Generated images: the script host keeps every buffer the run produced.
    if (app.scriptHost && !app.scriptHost->Images().empty()) {
        const auto& images = app.scriptHost->Images();
        const auto& names = app.scriptHost->ImageNames();
        app.script.imageIndex = std::clamp(app.script.imageIndex, 0,
                                           static_cast<int>(images.size()) - 1);
        const image::Buffer& buffer = images[static_cast<std::size_t>(app.script.imageIndex)];
        const float previewSize = std::min(size.x - U(24.0f), U(112.0f));
        const ImVec2 previewPos(pos.x + U(12.0f), pos.y + size.y - U(previewSize) - U(34.0f));

        // Draw the thumbnail straight into the draw list (64x64 blocks): no texture plumbing,
        // and it renders identically in the headless preview tool.
        const int cells = 56;
        const float cellSize = previewSize / static_cast<float>(cells);
        for (int y = 0; y < cells; ++y) {
            for (int x = 0; x < cells; ++x) {
                const image::Color color =
                    buffer.SampleBilinear((static_cast<float>(x) + 0.5f) / cells,
                                          (static_cast<float>(y) + 0.5f) / cells);
                const ImU32 packed = IM_COL32(static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255),
                                              static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255),
                                              static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255),
                                              255);
                const ImVec2 cellMin(previewPos.x + cellSize * x, previewPos.y + cellSize * y);
                dl->AddRectFilled(cellMin, ImVec2(cellMin.x + cellSize + 0.5f,
                                                  cellMin.y + cellSize + 0.5f),
                                  packed);
            }
        }
        dl->AddRect(previewPos, ImVec2(previewPos.x + previewSize, previewPos.y + previewSize),
                    Colors().stroke, U(4.0f));
        dl->AddText(Text().tiny, Text().tiny->FontSize,
                    ImVec2(previewPos.x, previewPos.y - U(14.0f)), Colors().textFaint,
                    names[static_cast<std::size_t>(app.script.imageIndex)].c_str());

        ImGui::SetCursorScreenPos(ImVec2(previewPos.x + previewSize + U(10.0f),
                                         previewPos.y + previewSize - U(26.0f)));
        if (ui::LinkButton("next", ui::Icon::ChevronRight)) {
            app.script.imageIndex = (app.script.imageIndex + 1) % static_cast<int>(images.size());
        }
        ImGui::SetCursorScreenPos(ImVec2(previewPos.x + previewSize + U(10.0f), previewPos.y));
        if (ui::LinkButton("save", ui::Icon::Download)) {
            const std::string directory =
                app.script.directory.empty() ? paths::EngineDataDir() : app.script.directory;
            fs::CreateDirectories(directory);
            const std::string path = paths::Join(
                directory, str::Format("image-%d.png", app.script.imageIndex + 1));
            if (image::WritePng(buffer, path)) {
                app.script.status = "Saved " + path;
                app.SetFooterStatus("Saved " + path);
            } else {
                app.PushToast(ToastKind::Error, "Could not save image", path);
            }
        }
    }
}

}  // namespace

void ScriptEditor(AstraApp& app) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = viewport->Pos;
    const ImVec2 size = viewport->Size;

    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), Colors().bg);

    // ---- top bar -----------------------------------------------------------
    const ImVec2 topMax(origin.x + size.x, origin.y + U(kTopBar));
    dl->AddRectFilled(origin, topMax, Colors().editorHeader);
    dl->AddLine(ImVec2(origin.x, topMax.y), ImVec2(topMax.x, topMax.y), Colors().stroke, U(1.0f));

    ImGui::SetCursorScreenPos(ImVec2(origin.x + U(10.0f), origin.y + U(9.0f)));
    if (ui::IconButton("##script_back", ui::Icon::ChevronLeft,
                       app.editor.project.isValid() ? "Back to the level editor" : "Back to the hub",
                       false, 28.0f)) {
        app.Goto(app.editor.project.isValid() ? Screen::Editor : Screen::ProjectHub);
    }

    dl->AddText(Text().medium, Text().medium->FontSize, ImVec2(origin.x + U(46.0f), origin.y + U(7.0f)),
                Colors().text, app.script.fileName.c_str());
    std::string meta = app.script.fromRepo ? "Repository scripting" : "AstraScript";
    if (!app.script.directory.empty()) meta += "  ·  " + app.script.directory;
    if (app.script.dirty) meta += "  ·  unsaved";
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(origin.x + U(46.0f), origin.y + U(26.0f)),
                Colors().textFaint, str::Ellipsis(meta, 96).c_str());

    // Transport on the right.
    float right = origin.x + size.x - U(12.0f);
    ImGui::SetCursorScreenPos(ImVec2(right - U(96.0f), origin.y + U(8.0f)));
    if (ui::PrimaryButton("Save", ImVec2(U(96.0f), U(30.0f)), true, ui::Icon::Save)) {
        app.SaveScript();
    }
    right -= U(104.0f);
    ImGui::SetCursorScreenPos(ImVec2(right - U(126.0f), origin.y + U(8.0f)));
    if (ui::SecondaryButton("Run  Ctrl+Enter", ImVec2(U(126.0f), U(30.0f)), true, ui::Icon::Play)) {
        app.SaveScript();
        app.RunScript();
    }

    // ---- body --------------------------------------------------------------
    const float bodyTop = origin.y + U(kTopBar) + U(kGap);
    const float bodyHeight = size.y - U(kTopBar) - U(kStatusBar) - U(kGap) * 2.0f;
    const float fileWidth = U(190.0f);
    const float outputWidth = U(300.0f);
    const float editorWidth = size.x - fileWidth - outputWidth - U(kGap) * 4.0f;
    const float editorX = origin.x + U(kGap) + fileWidth + U(kGap);

    DrawFileColumn(app, ImVec2(origin.x + U(kGap), bodyTop), ImVec2(fileWidth, bodyHeight));

    // Editor surface.
    const ImVec2 editorPos(editorX, bodyTop);
    const ImVec2 editorSize(editorWidth, bodyHeight);
    dl->AddRectFilled(editorPos, ImVec2(editorPos.x + editorSize.x, editorPos.y + editorSize.y),
                      Colors().surfaceSunken, U(8.0f));
    dl->AddRect(editorPos, ImVec2(editorPos.x + editorSize.x, editorPos.y + editorSize.y),
                Colors().stroke, U(8.0f), 0, U(1.0f));
    dl->AddRectFilled(editorPos, ImVec2(editorPos.x + editorSize.x, editorPos.y + U(30.0f)),
                      Colors().editorHeader, U(8.0f), ImDrawFlags_RoundCornersTop);

    ImGui::SetCursorScreenPos(ImVec2(editorPos.x + U(12.0f), editorPos.y + U(8.0f)));
    ui::PushFont(Text().small);
    ui::Text(Text().small, Colors().textDim, "%s", app.script.filePath.empty()
                                                      ? "unsaved buffer"
                                                      : app.script.filePath.c_str());
    ui::PopFont();

    // The text input owns the editing, but while it is idle we paint a syntax-highlighted copy
    // over it (and hide its own text) so the buffer looks like a real code editor.
    static bool sEditingBuffer = false;
    const float lineHeight = ImGui::GetTextLineHeight();
    const int lineCount = 1 + static_cast<int>(std::count(app.script.source.begin(),
                                                          app.script.source.end(), '\n'));
    const ImVec2 codePos(editorPos.x + U(10.0f), editorPos.y + U(34.0f));
    const float gutterWidth = U(38.0f);

    ImGui::SetCursorScreenPos(codePos);
    ImGui::BeginChild("##code_view", ImVec2(editorSize.x - U(18.0f), editorSize.y - U(42.0f)),
                      false, ImGuiWindowFlags_NoBackground);
    const ImVec2 childOrigin = ImGui::GetCursorScreenPos();
    const float codeWidth = editorSize.x - gutterWidth - U(40.0f);

    ImGui::SetCursorScreenPos(ImVec2(childOrigin.x + gutterWidth + U(6.0f), childOrigin.y));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          sEditingBuffer ? ui::ToVec4(Colors().text) : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ui::PushFont(Text().monoSmall);
    const ImVec2 inputSize(codeWidth, editorSize.y - U(58.0f));
    // ImGui takes char*: keep a scratch copy and write it back when it changes.
    std::vector<char> buffer(app.script.source.begin(), app.script.source.end());
    buffer.resize(512 * 1024, '\0');
    buffer[app.script.source.size()] = '\0';
    const bool edited = ImGui::InputTextMultiline("##script_source", buffer.data(), buffer.size(),
                                                  inputSize, ImGuiInputTextFlags_AllowTabInput);
    sEditingBuffer = ImGui::IsItemActive();
    ui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    if (edited) {
        app.script.source = buffer.data();
        app.script.dirty = true;
    }

    // Gutter + highlighted text (only while not typing, so the caret stays visible).
    ImVec2 linePos(childOrigin.x + gutterWidth + U(6.0f), childOrigin.y + U(1.0f));
    for (int i = 0; i < lineCount; ++i) {
        const std::size_t start = app.script.source.find_first_of('\n', 0);
        (void)start;
        if (linePos.y > childOrigin.y + inputSize.y - lineHeight) break;
        const bool isError = app.script.errorLine == i + 1;
        if (isError) {
            dl->AddRectFilled(ImVec2(childOrigin.x, linePos.y),
                              ImVec2(childOrigin.x + editorSize.x - U(28.0f),
                                     linePos.y + lineHeight),
                              ui::Fade(Colors().danger, 0.12f), U(3.0f));
        }
        char number[16];
        std::snprintf(number, sizeof(number), "%d", i + 1);
        dl->AddText(Text().monoSmall, Text().monoSmall->FontSize,
                    ImVec2(childOrigin.x + U(6.0f), linePos.y),
                    isError ? Colors().danger : Colors().textFaint, number);

        // Slice out this source line.
        std::size_t cursor = 0;
        for (int skip = 0; skip < i && cursor < app.script.source.size(); ++skip) {
            cursor = app.script.source.find('\n', cursor);
            if (cursor == std::string::npos) {
                cursor = app.script.source.size();
                break;
            }
            ++cursor;
        }
        if (cursor >= app.script.source.size()) break;
        const std::size_t eol = app.script.source.find('\n', cursor);
        const std::string line = app.script.source.substr(
            cursor, eol == std::string::npos ? std::string::npos : eol - cursor);

        if (!sEditingBuffer) {
            const int rows = DrawHighlightedLine(dl, Text().monoSmall, linePos, line,
                                                 codeWidth - U(12.0f), lineHeight);
            linePos.y += lineHeight * static_cast<float>(rows);
        } else {
            linePos.y += lineHeight;
        }
    }
    ImGui::EndChild();

    DrawOutputColumn(app, ImVec2(origin.x + size.x - U(kGap) - outputWidth, bodyTop),
                     ImVec2(outputWidth, bodyHeight));

    // ---- status bar --------------------------------------------------------
    const ImVec2 statusMin(origin.x, origin.y + size.y - U(kStatusBar));
    dl->AddRectFilled(statusMin, ImVec2(origin.x + size.x, origin.y + size.y), Colors().editorHeader);
    dl->AddLine(statusMin, ImVec2(origin.x + size.x, statusMin.y), Colors().stroke, U(1.0f));
    dl->AddText(Text().tiny, Text().tiny->FontSize, ImVec2(statusMin.x + U(12.0f), statusMin.y + U(7.0f)),
                app.script.error.empty() ? Colors().textDim : Colors().danger,
                str::Ellipsis(app.script.status.empty() ? "Ready" : app.script.status, 120).c_str());
    char stats[96];
    std::snprintf(stats, sizeof(stats), "%d lines  ·  %lld steps  ·  %.1f ms", lineCount,
                  app.script.lastSteps, app.script.lastMilliseconds);
    const float statsWidth =
        Text().tiny->CalcTextSizeA(Text().tiny->FontSize, FLT_MAX, 0.0f, stats).x;
    dl->AddText(Text().tiny, Text().tiny->FontSize,
                ImVec2(statusMin.x + size.x - statsWidth - U(16.0f), statusMin.y + U(7.0f)),
                Colors().textFaint, stats);

    // ---- shortcuts ---------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
        app.SaveScript();
        app.RunScript();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) app.SaveScript();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantTextInput) {
        app.Goto(app.editor.project.isValid() ? Screen::Editor : Screen::ProjectHub);
    }
}

}  // namespace astra::app::screens
