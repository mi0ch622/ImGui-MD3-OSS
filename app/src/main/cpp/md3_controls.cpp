// md3_controls.cpp - MD3 styled ImGui widgets: Card, Checkbox, RadioButton, Slider, Button, Combo
#include "md3_common.h"
#include <cstring>
#include <cstdio>

void Md3Card(const char* id, const ImVec4& bg) {
    float cardDelay = (float)g_cardCounter * 0.06f;
    float cardAnim = (g_itemSwitchAnim - cardDelay) / 0.3f;
    if (cardAnim < 0.0f) cardAnim = 0.0f;
    if (cardAnim > 1.0f) cardAnim = 1.0f;
    cardAnim = EaseOutCubic(cardAnim);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, cardAnim);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 14.0f));
    ImGui::BeginChild(
        id,
        ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    g_cardCounter++;
}

void Md3CardEnd() {
    ImGui::EndChild();

    ImVec2 cMin = ImGui::GetItemRectMin();
    ImVec2 cMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(
        ImVec2(cMin.x + 2, cMin.y + 4),
        ImVec2(cMax.x + 4, cMax.y + 8),
        Md3U32(Md3(0, 0, 0, 0.07f)), 14.0f);

    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 10.0f));
}

void PageHeader(const char* title, const char* subtitle) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& OnSurface = CurPalette().OnSurface;

    ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
    ImGui::SetWindowFontScale(1.4f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    {
        ImVec2 textMin = ImGui::GetItemRectMin();
        float textW = ImGui::CalcTextSize(title).x * 1.4f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(textMin.x, textMin.y), ImVec2(textMin.x + textW, textMin.y + 3.0f),
            Md3U32(Primary), 1.5f);
    }

    if (subtitle) {
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.78f);
        ImGui::TextWrapped("%s", subtitle);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, 4.0f));
}

bool Md3Checkbox(const char* label, bool* v) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnPrimary = CurPalette().OnPrimary;
    const ImVec4& OutlineVariant = CurPalette().OutlineVariant;
    const ImVec4& SurfaceVariant = CurPalette().SurfaceVariant;
    const ImVec4& OnSurface = CurPalette().OnSurface;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
    bool ret = ImGui::Checkbox(label, v);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float boxSize = 24.0f;
    ImVec2 boxMin(min.x, min.y + (max.y - min.y - boxSize) * 0.5f);
    ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
    bool hovered = ImGui::IsItemHovered();

    float anim = AnimateSpring(ImGui::GetID(label), *v, ImGui::GetIO().DeltaTime);
    if (anim < 0.0f) anim = 0.0f;
    if (anim > 1.0f) anim = 1.0f;

    if (*v) {
        dl->AddRectFilled(boxMin, boxMax, Md3U32(Primary), 8.0f);
        float cx = boxMin.x + boxSize * 0.5f;
        float cy = boxMin.y + boxSize * 0.5f;
        float r = boxSize * 0.3f;
        ImU32 checkCol = Md3U32(OnPrimary);
        float seg1 = (anim < 0.5f) ? anim / 0.5f : 1.0f;
        ImVec2 p1a(cx - r, cy);
        ImVec2 p1b(cx - r*0.2f, cy + r*0.7f);
        dl->AddLine(p1a, ImVec2(p1a.x + (p1b.x - p1a.x) * seg1, p1a.y + (p1b.y - p1a.y) * seg1), checkCol, 3.0f);
        if (anim > 0.5f) {
            float seg2 = (anim - 0.5f) / 0.5f;
            ImVec2 p2a = p1b;
            ImVec2 p2b(cx + r, cy - r*0.6f);
            dl->AddLine(p2a, ImVec2(p2a.x + (p2b.x - p2a.x) * seg2, p2a.y + (p2b.y - p2a.y) * seg2), checkCol, 3.0f);
        }
    } else {
        dl->AddRect(boxMin, boxMax, Md3U32(OutlineVariant), 8.0f, 0, 2.5f);
        if (anim > 0.0f) {
            ImVec4 fadeFill = Primary;
            fadeFill.w = anim * 0.35f;
            dl->AddRectFilled(boxMin, boxMax, Md3U32(fadeFill), 8.0f);
        }
    }
    if (hovered) {
        ImVec4 sl = *v ? Primary : OnSurface;
        sl.w = 0.08f;
        dl->AddRectFilled(boxMin, boxMax, Md3U32(sl), 8.0f);
    }
    return ret;
}

bool Md3RadioButton(const char* label, int* v, int v_button) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnSurface = CurPalette().OnSurface;
    const ImVec4& OutlineVariant = CurPalette().OutlineVariant;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
    bool ret = ImGui::RadioButton(label, v, v_button);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float d = 24.0f;
    ImVec2 center(min.x + d * 0.5f, min.y + (max.y - min.y) * 0.5f);
    bool hovered = ImGui::IsItemHovered();
    bool sel = (*v == v_button);

    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "%s##radio%d", label, v_button);
    float anim = AnimateSpring(ImGui::GetID(idBuf), sel, ImGui::GetIO().DeltaTime);
    if (anim < 0.0f) anim = 0.0f;
    if (anim > 1.0f) anim = 1.0f;

    if (sel) {
        dl->AddCircle(center, d * 0.5f, Md3U32(Primary), 32, 2.5f);
        float innerR = d * 0.24f * anim;
        if (innerR > 0.5f) dl->AddCircleFilled(center, innerR, Md3U32(Primary), 20);
    } else {
        dl->AddCircle(center, d * 0.5f, Md3U32(OutlineVariant), 32, 2.5f);
        if (anim > 0.0f) {
            dl->AddCircleFilled(center, d * 0.24f * anim, Md3U32(Primary), 20);
        }
    }
    if (hovered) {
        ImVec4 sl = sel ? Primary : OnSurface;
        sl.w = 0.08f;
        dl->AddCircleFilled(center, d * 0.5f, Md3U32(sl), 32);
    }
    return ret;
}

bool Md3SliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& PrimaryContainer = CurPalette().PrimaryContainer;
    const ImVec4& SurfaceVariant = CurPalette().SurfaceVariant;
    const ImVec4& OnSurface = CurPalette().OnSurface;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;

    bool hasLabel = !(label[0] == '#' && label[1] == '#');
    float labelW = hasLabel ? ImGui::CalcTextSize(label).x + 10.0f : 0.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Md3(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 16));
    bool ret = ImGui::SliderFloat(label, v, v_min, v_max, "");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float frameRight = max.x - labelW - 6.0f;
    float frameLeft = min.x + 6.0f;
    float trackY = (min.y + max.y) * 0.5f;
    float trackH = 6.0f;
    float availW = frameRight - frameLeft;
    float t = (*v - v_min) / (v_max - v_min);
    if (t < 0) t = 0; if (t > 1) t = 1;
    float handleX = frameLeft + availW * t;
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    if (availW > 0) {
        dl->AddRectFilled(ImVec2(frameLeft, trackY - trackH*0.5f), ImVec2(frameRight, trackY + trackH*0.5f),
            Md3U32(SurfaceVariant), trackH * 0.5f);
        if (handleX > frameLeft) {
            dl->AddRectFilled(ImVec2(frameLeft, trackY - trackH*0.5f), ImVec2(handleX, trackY + trackH*0.5f),
                Md3U32(Primary), trackH * 0.5f);
        }
    }
    float handleR = active ? 14.0f : (hovered ? 12.0f : 10.0f);
    if (active) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR + 6.0f,
            Md3U32(ImVec4(PrimaryContainer.x, PrimaryContainer.y, PrimaryContainer.z, 0.35f)), 28);
    } else if (hovered) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR + 4.0f,
            Md3U32(ImVec4(Primary.x, Primary.y, Primary.z, 0.08f)), 28);
    }
    ImU32 handleCol = active ? Md3U32(PrimaryContainer) : Md3U32(Primary);
    dl->AddCircleFilled(ImVec2(handleX, trackY), handleR, handleCol, 28);
    if (active) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR * 0.35f, Md3U32(Primary), 16);
    }

    {
        char buf[64];
        snprintf(buf, sizeof(buf), fmt, *v);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        ImVec2 tp(handleX - ts.x * 0.5f, trackY - 34.0f);
        float minX = frameLeft;
        float maxX = frameRight - ts.x;
        if (tp.x < minX) tp.x = minX;
        if (tp.x > maxX) tp.x = maxX;
        ImVec2 bmin(tp.x - 6.0f, tp.y - 3.0f);
        ImVec2 bmax(tp.x + ts.x + 6.0f, tp.y + ts.y + 3.0f);
        dl->AddRectFilled(bmin, bmax, Md3U32(SurfaceContainerHigh), 8.0f);
        dl->AddTriangleFilled(
            ImVec2(handleX - 4.0f, bmax.y),
            ImVec2(handleX + 4.0f, bmax.y),
            ImVec2(handleX, bmax.y + 5.0f),
            Md3U32(SurfaceContainerHigh));
        dl->AddText(tp, Md3U32(OnSurface), buf);
    }
    return ret;
}

bool Md3SliderInt(const char* label, int* v, int v_min, int v_max, const char* fmt) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& PrimaryContainer = CurPalette().PrimaryContainer;
    const ImVec4& SurfaceVariant = CurPalette().SurfaceVariant;
    const ImVec4& OnSurface = CurPalette().OnSurface;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;

    bool hasLabel = !(label[0] == '#' && label[1] == '#');
    float labelW = hasLabel ? ImGui::CalcTextSize(label).x + 10.0f : 0.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Md3(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Md3(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 16));
    bool ret = ImGui::SliderInt(label, v, v_min, v_max, "");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float frameRight = max.x - labelW - 6.0f;
    float frameLeft = min.x + 6.0f;
    float trackY = (min.y + max.y) * 0.5f;
    float trackH = 6.0f;
    float availW = frameRight - frameLeft;
    float t = (float)(*v - v_min) / (float)(v_max - v_min);
    if (t < 0) t = 0; if (t > 1) t = 1;
    float handleX = frameLeft + availW * t;
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    if (availW > 0) {
        dl->AddRectFilled(ImVec2(frameLeft, trackY - trackH*0.5f), ImVec2(frameRight, trackY + trackH*0.5f),
            Md3U32(SurfaceVariant), trackH * 0.5f);
        if (handleX > frameLeft) {
            dl->AddRectFilled(ImVec2(frameLeft, trackY - trackH*0.5f), ImVec2(handleX, trackY + trackH*0.5f),
                Md3U32(Primary), trackH * 0.5f);
        }
    }
    float handleR = active ? 14.0f : (hovered ? 12.0f : 10.0f);
    if (active) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR + 6.0f,
            Md3U32(ImVec4(PrimaryContainer.x, PrimaryContainer.y, PrimaryContainer.z, 0.35f)), 28);
    } else if (hovered) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR + 4.0f,
            Md3U32(ImVec4(Primary.x, Primary.y, Primary.z, 0.08f)), 28);
    }
    ImU32 handleCol = active ? Md3U32(PrimaryContainer) : Md3U32(Primary);
    dl->AddCircleFilled(ImVec2(handleX, trackY), handleR, handleCol, 28);
    if (active) {
        dl->AddCircleFilled(ImVec2(handleX, trackY), handleR * 0.35f, Md3U32(Primary), 16);
    }

    {
        char buf[64];
        snprintf(buf, sizeof(buf), fmt, *v);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        ImVec2 tp(handleX - ts.x * 0.5f, trackY - 34.0f);
        float minX = frameLeft;
        float maxX = frameRight - ts.x;
        if (tp.x < minX) tp.x = minX;
        if (tp.x > maxX) tp.x = maxX;
        ImVec2 bmin(tp.x - 6.0f, tp.y - 3.0f);
        ImVec2 bmax(tp.x + ts.x + 6.0f, tp.y + ts.y + 3.0f);
        dl->AddRectFilled(bmin, bmax, Md3U32(SurfaceContainerHigh), 8.0f);
        dl->AddTriangleFilled(
            ImVec2(handleX - 4.0f, bmax.y),
            ImVec2(handleX + 4.0f, bmax.y),
            ImVec2(handleX, bmax.y + 5.0f),
            Md3U32(SurfaceContainerHigh));
        dl->AddText(tp, Md3U32(OnSurface), buf);
    }
    return ret;
}

bool Md3Button(const char* label, const ImVec2& size_arg) {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnPrimary = CurPalette().OnPrimary;

    ImGui::PushStyleColor(ImGuiCol_Button, Primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Primary);
    ImGui::PushStyleColor(ImGuiCol_Text, OnPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 12));

    // 标签里 "##" 之后是 ID, 不显示; 显示文字单独画 → 解决 CJK 字体基线偏下
    char vis[128]; vis[0] = 0;
    char idOnly[160];
    const char* pp = strstr(label, "##");
    if (pp) {
        int n = (int)(pp - label); if (n > 120) n = 120;
        for (int i = 0; i < n; ++i) vis[i] = label[i]; vis[n] = 0;
        snprintf(idOnly, sizeof(idOnly), "##%s", pp + 2);
    } else {
        snprintf(vis, sizeof(vis), "%s", label);
        snprintf(idOnly, sizeof(idOnly), "###btn_%s", label);
    }
    const ImVec2 textSize = ImGui::CalcTextSize(vis);
    ImVec2 useSize = size_arg;
    if (useSize.x <= 0.0f) useSize.x = textSize.x + 32.0f;
    if (useSize.y <= 0.0f) useSize.y = textSize.y + 24.0f;

    bool ret = ImGui::Button(idOnly, useSize);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (active) {
        dl->AddRectFilled(min, max, Md3U32(ImVec4(0,0,0,0.2f)), 20.0f);
    } else if (hovered) {
        dl->AddRectFilled(min, max, Md3U32(ImVec4(1,1,1,0.12f)), 20.0f);
    }

    // 手动把文字画在按钮正中 (比 ImGui 自带对 CJK 更准)
    if (vis[0]) {
        const ImVec2 tp(min.x + (max.x - min.x - textSize.x) * 0.5f,
                        min.y + (max.y - min.y - textSize.y) * 0.5f);
        dl->AddText(tp, Md3U32(OnPrimary), vis);
    }
    return ret;
}

bool Md3Combo(const char* label, int* current_item, const char* items_separated_by_zeros) {
    const ImVec4& SurfaceVariant = CurPalette().SurfaceVariant;
    const ImVec4& OnSurface = CurPalette().OnSurface;
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& PrimaryContainer = CurPalette().PrimaryContainer;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, SurfaceVariant);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, SurfaceVariant);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, SurfaceVariant);
    ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, SurfaceContainerHigh);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PrimaryContainer);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, PrimaryContainer);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 12));
    bool ret = ImGui::Combo(label, current_item, items_separated_by_zeros);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(7);

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float arrowX = max.x - 18.0f;
    float arrowY = (min.y + max.y) * 0.5f;
    float aw = 5.0f;
    dl->AddLine(ImVec2(arrowX - aw, arrowY - aw*0.5f), ImVec2(arrowX, arrowY + aw*0.5f), Md3U32(OnSurfaceVariant), 2.0f);
    dl->AddLine(ImVec2(arrowX, arrowY + aw*0.5f), ImVec2(arrowX + aw, arrowY - aw*0.5f), Md3U32(OnSurfaceVariant), 2.0f);
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        dl->AddLine(ImVec2(min.x, max.y - 1), ImVec2(max.x, max.y - 1), Md3U32(Primary), 2.0f);
    }
    return ret;
}


// ---------------------------------------------------------------------
//  二级导航（胶囊标签条）: 用于「战斗 -> 战斗 / 美化」这类页内切换
// ---------------------------------------------------------------------
void Md3SubNav(const char* id, const char* const* items, int count, int* current) {
    if (items == nullptr || count <= 0 || current == nullptr) return;
    const Md3Palette& p = CurPalette();
    ImGui::PushID(id);
    for (int i = 0; i < count; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, 8.0f);
        const bool sel = (*current == i);
        ImVec4 bg = p.Primary;
        bg.w = sel ? 0.16f : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(p.Primary.x, p.Primary.y, p.Primary.z, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(p.Primary.x, p.Primary.y, p.Primary.z, 0.24f));
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? p.Primary : p.OnSurfaceVariant);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18.0f, 9.0f));
        if (ImGui::Button(items[i])) *current = i;
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PopID();
}

// ---------------------------------------------------------------------
//  美化卡片控件（对应 Lua 里的 baishan.MH 那个组合控件）:
//    名称文本  |  当前序号/总数量 数字步进器 + 【-】【+】
//    皮肤副标题 | 【美化】动作按钮
//  返回 true 表示本帧按下了「美化」。
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
//  美化卡片控件 v2（重做视觉）
//
//   ┌───────────────────────────────────────────────────────────┐
//   │  ╭────╮   AK47            ╭───────────────╮  ┌────────┐   │
//   │  │ A  │   火麒麟           │ −   3/5    +  │  │  美化   │   │
//   │  ╰────╯                   ╰───────────────╯  └────────┘   │
//   └───────────────────────────────────────────────────────────┘
//
//   左 : monogram 圆角方块 + 名称 / 皮肤副标题（两行，整体垂直居中）
//   中 : 分段式数字步进器（胶囊底，− / 数字 / +，矢量细线图标）
//   右 : 主色填充「美化」按钮（带投影，按下变深）
//   返回 true 表示按下了「美化」。
// ---------------------------------------------------------------------
bool Md3BeautifyCard(const char* id, const char* name, const char* const* skins,
                     int skinCount, int* idx) {
    if (idx == nullptr || skins == nullptr || skinCount <= 0) return false;
    if (*idx < 0) *idx = 0;
    if (*idx >= skinCount) *idx = skinCount - 1;

    const Md3Palette& p = CurPalette();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont*     f  = ImGui::GetFont();
    const float base = ImGui::GetFontSize();

    // ------------------------- 尺寸 -------------------------
    const float H     = 104.0f;   // 卡片高
    const float padX  = 20.0f;    // 左右内边距
    const float tile  = 56.0f;    // monogram 方块边长
    const float segW  = 46.0f;    // − / + 段宽
    const float cW    = 58.0f;    // 数字段宽
    const float ctlH  = 40.0f;    // 控件高
    const float stepW = segW + cW + segW;
    const float btnW  = 96.0f;
    const float gap   = 10.0f;

    ImGui::PushID(id);

    const ImVec2 p0     = ImGui::GetCursorScreenPos();
    const float  availW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p1     = ImVec2(p0.x + availW, p0.y + H);
    const bool   cardHover = ImGui::IsMouseHoveringRect(p0, p1, false);

    // ------------------------- 卡片底 + 软阴影 -------------------------
    dl->AddRectFilled(ImVec2(p0.x + 2.0f, p0.y + 3.0f),
                      ImVec2(p1.x + 3.0f, p1.y + 7.0f),
                      Md3U32(Md3(0, 0, 0, 0.06f)), 20.0f);
    {
        ImVec4 bg = p.SurfaceContainerHigh;
        if (cardHover) { bg.x += 0.02f; bg.y += 0.02f; bg.z += 0.02f; }
        dl->AddRectFilled(p0, p1, Md3U32(bg), 20.0f);
    }

    // ------------------------- 右侧控件位置 -------------------------
    const float rightEdge = p1.x - padX;
    const float btnX      = rightEdge - btnW;
    const float stepX     = btnX - gap - stepW;
    const float ctlY      = p0.y + (H - ctlH) * 0.5f;

    // ------------------------- 左侧 monogram 方块 -------------------------
    const ImVec2 t0(p0.x + padX, p0.y + (H - tile) * 0.5f);
    const ImVec2 t1(t0.x + tile, t0.y + tile);
    {
        ImVec4 tileBg = p.Primary;
        tileBg.w = 0.14f;
        dl->AddRectFilled(t0, t1, Md3U32(tileBg), 16.0f);

        char mono[8] = { 0 };
        if (name != nullptr && name[0] != 0) {
            const unsigned char c0 = (unsigned char)name[0];
            int n = (c0 < 0x80) ? 1 : ((c0 >> 5) == 0x6 ? 2 : ((c0 >> 4) == 0xE ? 3 : 4));
            for (int i = 0; i < n && i < 7 && name[i] != 0; ++i) mono[i] = name[i];
        }
        const float ms = base * 1.05f;
        const ImVec2 mw = f->CalcTextSizeA(ms, FLT_MAX, 0.0f, mono);
        dl->AddText(f, ms,
                    ImVec2(t0.x + (tile - mw.x) * 0.5f, t0.y + (tile - mw.y) * 0.5f),
                    Md3U32(p.Primary), mono);
    }

    // ------------------------- 中间文字（名 / 皮肤） -------------------------
    {
        const float tx = t1.x + 16.0f;
        dl->PushClipRect(ImVec2(tx, p0.y), ImVec2(stepX - 10.0f, p1.y), true);

        const float nameSize = base * 0.95f;
        const float subSize  = base * 0.78f;
        const ImVec2 nw = f->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, name);
        const ImVec2 sw = f->CalcTextSizeA(subSize,  FLT_MAX, 0.0f, skins[*idx]);
        const float  gapY   = 7.0f;
        const float  totalH = nw.y + gapY + sw.y;
        const float  ty0    = p0.y + (H - totalH) * 0.5f;

        dl->AddText(f, nameSize, ImVec2(tx, ty0),
                    Md3U32(p.OnSurface), name);
        dl->AddText(f, subSize, ImVec2(tx, ty0 + nw.y + gapY),
                    Md3U32(p.OnSurfaceVariant), skins[*idx]);
        dl->PopClipRect();
    }

    // ------------------------- 数字步进器（胶囊） -------------------------
    bool hitMinus = false, hitPlus = false;
    {
        ImVec4 pillBg = p.Primary;
        pillBg.w = 0.10f;
        dl->AddRectFilled(ImVec2(stepX, ctlY), ImVec2(stepX + stepW, ctlY + ctlH),
                          Md3U32(pillBg), ctlH * 0.5f);

        const bool canMinus = (*idx > 0);
        const bool canPlus  = (*idx < skinCount - 1);

        // ---- − ----
        ImGui::SetCursorScreenPos(ImVec2(stepX, ctlY));
        hitMinus = ImGui::InvisibleButton("##minus", ImVec2(segW, ctlH));
        {
            const bool h = ImGui::IsItemHovered(), a = ImGui::IsItemActive();
            const ImVec2 c(stepX + segW * 0.5f, ctlY + ctlH * 0.5f);
            if (h && canMinus)
                dl->AddCircleFilled(c, 16.0f,
                    Md3U32(ImVec4(p.Primary.x, p.Primary.y, p.Primary.z, a ? 0.30f : 0.18f)));
            ImVec4 gc = p.Primary;
            if (!canMinus) gc.w = 0.35f;
            dl->AddLine(ImVec2(c.x - 8.0f, c.y), ImVec2(c.x + 8.0f, c.y), Md3U32(gc), 2.2f);
        }

        // ---- 数字 ----
        {
            char cnt[40];
            snprintf(cnt, sizeof(cnt), "%d/%d", *idx + 1, skinCount);
            const float cs = base * 0.86f;
            const ImVec2 cw = f->CalcTextSizeA(cs, FLT_MAX, 0.0f, cnt);
            const ImVec2 c(stepX + segW + cW * 0.5f, ctlY + ctlH * 0.5f);
            dl->AddText(f, cs, ImVec2(c.x - cw.x * 0.5f, c.y - cw.y * 0.5f),
                        Md3U32(p.OnSurface), cnt);
        }

        // ---- + ----
        ImGui::SetCursorScreenPos(ImVec2(stepX + segW + cW, ctlY));
        hitPlus = ImGui::InvisibleButton("##plus", ImVec2(segW, ctlH));
        {
            const bool h = ImGui::IsItemHovered(), a = ImGui::IsItemActive();
            const ImVec2 c(stepX + segW + cW + segW * 0.5f, ctlY + ctlH * 0.5f);
            if (h && canPlus)
                dl->AddCircleFilled(c, 16.0f,
                    Md3U32(ImVec4(p.Primary.x, p.Primary.y, p.Primary.z, a ? 0.30f : 0.18f)));
            ImVec4 gc = p.Primary;
            if (!canPlus) gc.w = 0.35f;
            dl->AddLine(ImVec2(c.x - 8.0f, c.y), ImVec2(c.x + 8.0f, c.y), Md3U32(gc), 2.2f);
            dl->AddLine(ImVec2(c.x, c.y - 8.0f), ImVec2(c.x, c.y + 8.0f), Md3U32(gc), 2.2f);
        }
    }

    // ------------------------- 「美化」按钮 -------------------------
    bool apply = false;
    {
        ImGui::SetCursorScreenPos(ImVec2(btnX, ctlY));
        apply = ImGui::InvisibleButton("##apply", ImVec2(btnW, ctlH));
        const bool h = ImGui::IsItemHovered(), a = ImGui::IsItemActive();

        ImVec4 bg = p.Primary;
        if (a)      { bg.x *= 0.88f; bg.y *= 0.88f; bg.z *= 0.88f; }
        else if (h) { bg.x += 0.06f; bg.y += 0.06f; bg.z += 0.06f; }

        dl->AddRectFilled(ImVec2(btnX + 1.0f, ctlY + 2.0f),
                          ImVec2(btnX + btnW + 2.0f, ctlY + ctlH + 4.0f),
                          Md3U32(Md3(0, 0, 0, 0.14f)), ctlH * 0.5f);
        dl->AddRectFilled(ImVec2(btnX, ctlY), ImVec2(btnX + btnW, ctlY + ctlH),
                          Md3U32(bg), ctlH * 0.5f);

        const char* lbl = "美化";
        const float ls  = base * 0.88f;
        const ImVec2 lw = f->CalcTextSizeA(ls, FLT_MAX, 0.0f, lbl);
        dl->AddText(f, ls,
                    ImVec2(btnX + (btnW - lw.x) * 0.5f, ctlY + (ctlH - lw.y) * 0.5f),
                    Md3U32(p.OnPrimary), lbl);
    }

    // ------------------------- 应用数值变化 -------------------------
    if (hitMinus && *idx > 0)              --(*idx);
    if (hitPlus  && *idx < skinCount - 1)  ++(*idx);

    // ------------------------- 推进布局（卡高 + 12px 间距） -------------------------
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
    ImGui::Dummy(ImVec2(availW, 12.0f));

    ImGui::PopID();
    return apply;
}
