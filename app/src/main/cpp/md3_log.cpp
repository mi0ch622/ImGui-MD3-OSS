// md3_log.cpp - Log system with ring buffer and colored level bars
#include "md3_common.h"

#define LOG_MAX_ENTRIES 128
#define LOG_MSG_MAX 160

struct LogEntry {
    LogLevel  level;
    char      message[LOG_MSG_MAX];
};

static LogEntry g_logBuffer[LOG_MAX_ENTRIES];
static int      g_logCount = 0;
static int      g_logHead  = 0;
static bool     g_logScroll = true;

static const char* LogLevelName(LogLevel lv) {
    switch (lv) {
    case LOG_INIT: return "INIT";
    case LOG_INFO: return "INFO";
    case LOG_OK:   return "OK";
    case LOG_WARN: return "WARN";
    case LOG_ERR:  return "ERROR";
    }
    return "INFO";
}

void PushLog(LogLevel lv, const char* fmt, ...) {
    char buf[LOG_MSG_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    int idx = (g_logHead + g_logCount) % LOG_MAX_ENTRIES;
    if (g_logCount < LOG_MAX_ENTRIES) g_logCount++;
    else g_logHead = (g_logHead + 1) % LOG_MAX_ENTRIES;

    LogEntry& e = g_logBuffer[idx];
    e.level = lv;
    snprintf(e.message, sizeof(e.message), "%s", buf);
    g_logScroll = true;
}

static ImU32 LogColor(LogLevel lv) {
    switch (lv) {
    case LOG_OK:   return Md3U32(Md3(120, 210, 150));
    case LOG_WARN: return Md3U32(Md3(240, 185, 85));
    case LOG_ERR:  return Md3U32(Md3(240, 120, 120));
    case LOG_INIT: return Md3U32(CurPalette().Tertiary);
    default:       return Md3U32(CurPalette().Primary);
    }
}

void PageTerminal() {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnSurface = CurPalette().OnSurface;
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& OutlineVariant = CurPalette().OutlineVariant;

    ImGui::PushStyleColor(ImGuiCol_Text, Primary);
    ImGui::TextUnformatted(T(SK_LOG_TITLE));
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
    ImGui::TextWrapped("%s", T(SK_LOG_SUB));
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, SurfaceContainerHigh);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::BeginChild("log_panel", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float availW = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < g_logCount; i++) {
        const LogEntry& e = g_logBuffer[(g_logHead + i) % LOG_MAX_ENTRIES];
        ImU32 col = LogColor(e.level);
        const char* lvl = LogLevelName(e.level);

        ImGui::PushID(i);

        ImVec2 entryPos = ImGui::GetCursorScreenPos();
        ImVec2 labelSize = ImGui::CalcTextSize(lvl);
        float entryH = labelSize.y + 6.0f;

        bool isLatest = (i == g_logCount - 1);
        float slideX = 0.0f;
        float slideA = 1.0f;
        if (isLatest && g_logScroll) {
            static float slideTimer = 0.0f;
            slideTimer += ImGui::GetIO().DeltaTime;
            float t = slideTimer / 0.2f;
            if (t < 1.0f) {
                slideX = -20.0f * (1.0f - EaseOutCubic(t));
                slideA = EaseOutCubic(t);
            } else {
                slideX = 0.0f;
                slideA = 1.0f;
            }
        }

        dl->AddRectFilled(
            ImVec2(entryPos.x + slideX, entryPos.y),
            ImVec2(entryPos.x + 4.0f + slideX, entryPos.y + entryH),
            col, 2.0f);

        ImGui::SetCursorScreenPos(ImVec2(entryPos.x + 12.0f + slideX, entryPos.y + 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, slideA);
        ImGui::TextUnformatted(lvl);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, slideA);
        ImGui::TextUnformatted(e.message);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::PopID();

        if (i < g_logCount - 1) {
            ImVec2 sepMin = ImGui::GetCursorScreenPos();
            sepMin.y += 3.0f;
            ImVec2 sepMax(sepMin.x + availW, sepMin.y);
            dl->AddLine(sepMin, sepMax, Md3U32(OutlineVariant), 1.0f);
        }
    }

    if (g_logScroll) {
        ImGui::SetScrollHereY(1.0f);
        g_logScroll = false;
    }

    HandleTouchDragScroll(false);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
