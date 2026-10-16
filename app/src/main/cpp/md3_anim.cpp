// md3_anim.cpp - Spring physics, easing functions, and touch scroll helper
#include "md3_common.h"

int g_navIndex = 0;
float g_indicatorY = 0.0f;
float g_indicatorVelY = 0.0f;
float g_itemSwitchAnim = 0.0f;
bool g_indicatorInit = false;
int g_cardCounter = 0;

void SpringUpdate(float& pos, float& vel, float target, float dt,
                         float omega, float zeta) {
    float f = -omega * omega * (pos - target) - 2.0f * zeta * omega * vel;
    vel += f * dt;
    pos += vel * dt;
    if (fabsf(pos - target) < 0.5f && fabsf(vel) < 2.0f) {
        pos = target; vel = 0.0f;
    }
}

float EaseOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }

float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float AnimateSpring(ImGuiID id, bool target, float dt) {
    ImGuiStorage* s = ImGui::GetStateStorage();
    float goal = target ? 1.0f : 0.0f;
    float* pos = s->GetFloatRef(id, goal);
    float* vel = s->GetFloatRef(id + 0x100000, 0.0f);
    const float omega = 18.0f, zeta = 0.5f;
    float f = -omega * omega * (*pos - goal) - 2.0f * zeta * omega * (*vel);
    *vel += f * dt;
    *pos += *vel * dt;
    if (fabsf(*pos - goal) < 0.003f && fabsf(*vel) < 0.08f) {
        *pos = goal;
        *vel = 0.0f;
    }
    return *pos;
}

void HandleTouchDragScroll(bool includeChildWindows) {
    ImGuiHoveredFlags hoverFlags = includeChildWindows
        ? ImGuiHoveredFlags_ChildWindows
        : ImGuiHoveredFlags_None;
    ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsWindowHovered(hoverFlags) || ImGui::IsAnyItemActive()) return;
    if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) return;
    if (fabsf(io.MouseDelta.y) < 0.01f) return;

    float nextScroll = ImGui::GetScrollY() - io.MouseDelta.y * 1.35f;
    if (nextScroll < 0.0f) nextScroll = 0.0f;
    float maxScroll = ImGui::GetScrollMaxY();
    if (nextScroll > maxScroll) nextScroll = maxScroll;
    ImGui::SetScrollY(nextScroll);
}
