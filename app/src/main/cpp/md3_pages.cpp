// md3_pages.cpp - Page content (Home, Target, Globe, Settings) and main window RenderDemoFrame
#include "md3_common.h"
#include "md3_uicon.hpp"

char g_homeBuf[64] = "你好";
char g_termBuf[128] = "";
int g_setChoice = 0;
bool g_setNotify = true;
float g_setVolume = 0.6f;

float g_progressDemo = 0.35f;
int g_radioDemo = 0;
bool g_checkDemo1 = true;
bool g_checkDemo2 = false;
int g_comboDemo = 1;
float g_sliderDemo = 50.0f;
int g_counterDemo = 0;

bool g_combatEnabled = true;
float g_combatRange = 8.0f;
float g_combatSpeed = 1.5f;
int g_combatMode = 0;
bool g_combatAuto = false;
float g_combatFOV = 70.0f;

char g_worldAddr[64] = "192.168.1.1";
int g_worldPort = 19132;
bool g_worldConnected = false;
float g_worldLatency = 0.0f;
int g_worldProtocol = 0;

bool g_setAutoStart = false;
bool g_setHaptic = true;
float g_setBrightness = 0.8f;

void PageHome() {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& PrimaryContainer = CurPalette().PrimaryContainer;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;

    PageHeader(T(SK_HOME_TITLE), T(SK_HOME_SUB));

    // 网络与位置：VPN 状态 / 公网 IP / 详细归属地
    PageNetCard();

    Md3Card("##card_input", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_INPUT_BUTTONS));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##home", T(SK_INPUT_NAME_HINT), g_homeBuf, sizeof(g_homeBuf));
        {
            ImVec2 fMin = ImGui::GetItemRectMin();
            ImVec2 fMax = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            bool focused = ImGui::IsItemHovered() || ImGui::IsItemActive();
            ImU32 lineCol = focused ? Md3U32(Primary) : Md3U32(CurPalette().OutlineVariant);
            dl->AddLine(ImVec2(fMin.x, fMax.y - 1), ImVec2(fMax.x, fMax.y - 1), lineCol, 2.0f);
        }

        if (Md3Button(T(SK_SAY_HELLO), ImVec2(-1, 0))) {
            PushLog(LOG_INFO, "%s: %s", T(SK_SAY_HELLO), g_homeBuf);
            g_counterDemo++;
        }
        ImGui::SameLine();
        if (Md3Button(T(SK_COUNTER), ImVec2(-1, 0))) g_counterDemo++;
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::Text(T(SK_COUNT_FMT), g_counterDemo);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    Md3Card("##card_slider", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_SLIDER_PROGRESS));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("##slider_demo", &g_sliderDemo, 0.0f, 100.0f, "%.0f%%");
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("##progress", &g_progressDemo, 0.0f, 1.0f, T(SK_PROGRESS_FMT));
        {
            ImVec2 pMin = ImGui::GetCursorScreenPos();
            float pW = ImGui::GetContentRegionAvail().x;
            float pH = 8.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pMin, ImVec2(pMin.x + pW, pMin.y + pH),
                Md3U32(CurPalette().SurfaceVariant), pH * 0.5f);
            float fillW = pW * g_progressDemo;
            if (fillW > 0.0f) {
                dl->AddRectFilled(pMin, ImVec2(pMin.x + fillW, pMin.y + pH),
                    Md3U32(Primary), pH * 0.5f);
            }
            ImGui::Dummy(ImVec2(0, pH + 2.0f));
        }
    }
    Md3CardEnd();

    Md3Card("##card_select", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_SELECTION));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        Md3Checkbox(T(SK_ENABLE_NOTIFY), &g_checkDemo1);
        Md3Checkbox(T(SK_AUTO_SAVE), &g_checkDemo2);
        char optLabel[32];
        snprintf(optLabel, sizeof(optLabel), T(SK_OPTION), 'A');
        Md3RadioButton(optLabel, &g_radioDemo, 0); ImGui::SameLine();
        snprintf(optLabel, sizeof(optLabel), T(SK_OPTION), 'B');
        Md3RadioButton(optLabel, &g_radioDemo, 1); ImGui::SameLine();
        snprintf(optLabel, sizeof(optLabel), T(SK_OPTION), 'C');
        Md3RadioButton(optLabel, &g_radioDemo, 2);
        ImGui::SetNextItemWidth(-1.0f);
        Md3Combo("##combo_demo", &g_comboDemo, T(SK_COMBO_LMH));
    }
    Md3CardEnd();
}

// =====================================================================
//  「美化」页数据 —— 先用一份示例表，之后直接换成你真实的枪械/皮肤列表
//  （结构与你 Lua 里的 baishan.MH(gname, shuzhi, sname) 一一对应）
// =====================================================================
static const char* kSkins_AK47[]  = { "原厂", "火麒麟", "黄金AK", "冰霜巨龙", "黑武士" };
static const char* kSkins_M4A1[]  = { "原厂", "雷神", "银色杀手", "赤焰" };
static const char* kSkins_AWM[]   = { "原厂", "樱花", "暗夜刺客" };
static const char* kSkins_BARET[] = { "原厂", "龙纹" };
static const char* kSkins_USP[]   = { "原厂", "黑金" };

struct BeautifyItem {
    const char*        name;      // 名称文本（枪械原名）
    const char* const* skins;     // 皮肤名表（副标题按序号取）
    int                count;     // 总数量
    int                idx;       // 当前序号（0 基）
};

static BeautifyItem g_beautify[] = {
    { "AK47",   kSkins_AK47,  5, 0 },
    { "M4A1",   kSkins_M4A1,  4, 0 },
    { "AWM",    kSkins_AWM,   3, 0 },
    { "巴雷特", kSkins_BARET, 2, 0 },
    { "USP",    kSkins_USP,   2, 0 },
};
static const int g_beautifyCount = (int)(sizeof(g_beautify) / sizeof(g_beautify[0]));

static char   g_beautifyMsg[128] = { 0 };   // 最近一次操作提示
static double g_beautifyMsgT     = 0.0;

/** 应用皮肤：目前只做屏上提示；接真实写入时把这一句换掉即可 */
static void BeautifyApply(BeautifyItem& it) {
    snprintf(g_beautifyMsg, sizeof(g_beautifyMsg), "已应用：%s · %s", it.name, it.skins[it.idx]);
    g_beautifyMsgT = ImGui::GetTime();
}

/** 「美化」页：一列美化卡片 */
void PageBeautify() {
    PageHeader("美化", "选中枪械皮肤，一键替换");

    if (g_beautifyMsg[0] != 0 && ImGui::GetTime() - g_beautifyMsgT < 3.0) {
        ImGui::PushStyleColor(ImGuiCol_Text, CurPalette().Primary);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(g_beautifyMsg);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }

    for (int i = 0; i < g_beautifyCount; ++i) {
        char cid[32];
        snprintf(cid, sizeof(cid), "##beautify_%d", i);
        if (Md3BeautifyCard(cid, g_beautify[i].name, g_beautify[i].skins,
                            g_beautify[i].count, &g_beautify[i].idx)) {
            BeautifyApply(g_beautify[i]);
        }
    }
}

// =====================================================================
//  「绘制」(ESP) 模块
//  * 绘制项开关 / 方框样式 / 配色 / 实时预览画布
//  * 配色统一走 ImGui 内置调色板(ColorEdit4 弹窗)，没有自绘调色板
// =====================================================================

struct EspConfig {
    bool  enabled;
    bool  drawId;
    bool  drawDist;
    bool  drawBox;
    int   boxStyle;      // 0=2D方框  1=3D方框  2=霓虹框
    bool  drawSkeleton;
    bool  drawTracer;
    bool  drawHealth;
    float range;         // 绘制距离(米)
    float boxThick;      // 线宽(px)
    ImVec4 colBox;
    ImVec4 colSkel;
    ImVec4 colText;
    ImVec4 colTracer;
};

static void EspOverlayOnStand(ImDrawList* dl, ImVec2 a, ImVec2 b);   // 前置声明 (定义在文件后半)

static EspConfig g_esp = {
    false, true, true, true, 0, false, false, false, 60.0f, 2.0f,
    ImVec4(0.36f, 0.68f, 1.00f, 1.0f),   // 方框: 蓝
    ImVec4(1.00f, 0.82f, 0.35f, 1.0f),   // 骨骼: 黄
    ImVec4(1.00f, 1.00f, 1.00f, 1.0f),   // 文字: 白
    ImVec4(1.00f, 0.42f, 0.42f, 1.0f),   // 射线: 红
};

// ---- 自瞄范围圈(战斗页) ----
static bool   g_aimCircle = false;
static float  g_aimRadius = 160.0f;
static float  g_aimThick  = 1.5f;
static ImVec4 g_aimColor  = ImVec4(0.36f, 0.68f, 1.00f, 1.0f);

static float g_iconX = -1.0f, g_iconY = -1.0f;
static float g_iconDrag = 0.0f;
static bool  g_iconDragOn = false;

// 绘制(ESP)图标: 四个角标 + 中心圆点 (与 DrawIconTarget 的同心圆区分)
void DrawIconDraw(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected)
{
    const float t   = r * 0.16f;
    const float o   = r * 0.92f;
    const float arm = r * 0.48f;
    const float x0 = c.x - o, x1 = c.x + o;
    const float y0 = c.y - o, y1 = c.y + o;

    dl->AddLine(ImVec2(x0, y0), ImVec2(x0 + arm, y0), col, t);
    dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y0 + arm), col, t);
    dl->AddLine(ImVec2(x1 - arm, y0), ImVec2(x1, y0), col, t);
    dl->AddLine(ImVec2(x1, y0), ImVec2(x1, y0 + arm), col, t);
    dl->AddLine(ImVec2(x0, y1 - arm), ImVec2(x0, y1), col, t);
    dl->AddLine(ImVec2(x0, y1), ImVec2(x0 + arm, y1), col, t);
    dl->AddLine(ImVec2(x1 - arm, y1), ImVec2(x1, y1), col, t);
    dl->AddLine(ImVec2(x1, y1), ImVec2(x1, y1 - arm), col, t);

    dl->AddCircle(c, r * 0.26f, col, 16, t);
    if (selected) dl->AddCircleFilled(c, r * 0.11f, col, 12);
}

// ---------------------------------------------------------------------
//  颜色行: 左边标签 + 右边色块
// ---------------------------------------------------------------------
static void EspColorRow(const char* label, ImVec4& col, const ImVec4& textCol)
{
    ImGui::PushID(label);
    const float x0    = ImGui::GetCursorPosX();
    const float avail = ImGui::GetContentRegionAvail().x;

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::SameLine(x0 + avail - 180.0f);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::ColorEdit4("##pick", (float*)&col,
                      ImGuiColorEditFlags_NoInputs |
                      ImGuiColorEditFlags_AlphaPreviewHalf |
                      ImGuiColorEditFlags_AlphaBar |
                      ImGuiColorEditFlags_PickerHueWheel);
    ImGui::PopID();
}

// ---------------------------------------------------------------------
//  ESP 预览画布: 完全按当前开关 / 配色 / 线宽实时绘制，所见即所得
// ---------------------------------------------------------------------
static void EspPreviewCanvas(const ImVec4& bg, float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont*     f  = ImGui::GetFont();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p1(p0.x + w, p0.y + height);

    dl->PushClipRect(p0, p1, true);
    dl->AddRectFilled(p0, p1, Md3U32(bg), 12.0f);

    for (float x = p0.x + 24.0f; x < p1.x - 4.0f; x += 30.0f)
        dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), Md3U32(ImVec4(1, 1, 1, 0.045f)), 1.0f);
    for (float y = p0.y + 24.0f; y < p1.y - 4.0f; y += 30.0f)
        dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), Md3U32(ImVec4(1, 1, 1, 0.045f)), 1.0f);

    // ---- 示意角色: 人形关节 ----
    const float cx = (p0.x + p1.x) * 0.5f;
    const float uW = (w * 0.78f) / 4.5f;       // 方框横向占 4.5u（±2.25u）
    const float uH = (height * 0.62f) / 7.2f;  // 人形纵向约 7.2u
    const float u  = (uW < uH) ? uW : uH;
    const float baseY = p0.y + height * 0.5f + u * 3.5f;

    ImVec2 head(cx, baseY - u * 6.55f);
    ImVec2 neck(cx, baseY - u * 5.55f);
    ImVec2 shL(cx - u * 1.05f, neck.y + u * 0.35f);
    ImVec2 shR(cx + u * 1.05f, neck.y + u * 0.35f);
    ImVec2 elL(cx - u * 1.50f, neck.y + u * 1.80f);
    ImVec2 elR(cx + u * 1.50f, neck.y + u * 1.80f);
    ImVec2 haL(cx - u * 1.85f, neck.y + u * 3.15f);
    ImVec2 haR(cx + u * 1.85f, neck.y + u * 3.15f);
    ImVec2 hip(cx, baseY - u * 2.05f);
    ImVec2 knL(cx - u * 0.58f, baseY - u * 1.05f);
    ImVec2 knR(cx + u * 0.58f, baseY - u * 1.05f);
    ImVec2 ftL(cx - u * 0.68f, baseY);
    ImVec2 ftR(cx + u * 0.68f, baseY);

    // ---- 方框 ----
    if (g_esp.drawBox) {
        const ImVec4& c = g_esp.colBox;
        const float bx0 = cx - u * 2.25f, bx1 = cx + u * 2.25f;
        const float by0 = baseY - u * 7.02f, by1 = baseY + u * 0.12f;

        if (g_esp.boxStyle == 1) {
            // 3D 线框: 前后两个矩形 + 4 条连线
            const float dx = u * 0.52f, dy = u * 0.42f;
            const ImU32 cc = Md3U32(c);
            dl->AddRect(ImVec2(bx0 + dx, by0 - dy), ImVec2(bx1 + dx, by1 - dy), cc, 0.0f, 0, g_esp.boxThick);
            dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), cc, 0.0f, 0, g_esp.boxThick);
            dl->AddLine(ImVec2(bx0, by0), ImVec2(bx0 + dx, by0 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(bx1, by0), ImVec2(bx1 + dx, by0 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(bx0, by1), ImVec2(bx0 + dx, by1 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(bx1, by1), ImVec2(bx1 + dx, by1 - dy), cc, g_esp.boxThick);
        } else if (g_esp.boxStyle == 2) {
            // 霓虹框: 多层递减 alpha (手法取自 Minecraft_be_opengl 的 NeonBox)
            dl->AddRect(ImVec2(bx0 - 6, by0 - 6), ImVec2(bx1 + 6, by1 + 6), Md3U32(ImVec4(c.x, c.y, c.z, 0.05f)), 6.0f, 0, 5.0f);
            dl->AddRect(ImVec2(bx0 - 4, by0 - 4), ImVec2(bx1 + 4, by1 + 4), Md3U32(ImVec4(c.x, c.y, c.z, 0.10f)), 5.0f, 0, 4.0f);
            dl->AddRect(ImVec2(bx0 - 2, by0 - 2), ImVec2(bx1 + 2, by1 + 2), Md3U32(ImVec4(c.x, c.y, c.z, 0.28f)), 4.0f, 0, 3.0f);
            dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), Md3U32(c), 3.0f, 0, 1.6f);
        } else {
            // 2D 方框: 深色描边 + 主色细线(保证任何背景下都看得清)
            dl->AddRect(ImVec2(bx0 - 1.5f, by0 - 1.5f), ImVec2(bx1 + 1.5f, by1 + 1.5f),
                        Md3U32(ImVec4(0, 0, 0, 0.55f)), 2.0f, 0, g_esp.boxThick + 2.0f);
            dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), Md3U32(c), 2.0f, 0, g_esp.boxThick);
        }

        if (g_esp.drawHealth) {
            dl->AddRectFilled(ImVec2(bx0 - 9.0f, by0), ImVec2(bx0 - 4.0f, by1), Md3U32(ImVec4(0, 0, 0, 0.55f)), 3.0f);
            dl->AddRectFilled(ImVec2(bx0 - 8.0f, by0 + (by1 - by0) * 0.30f), ImVec2(bx0 - 5.0f, by1 - 1.0f),
                              Md3U32(ImVec4(0.36f, 0.88f, 0.42f, 1.0f)), 3.0f);
        }

        // ---- ID / 距离 (方框上方居中, 带黑色描边) ----
        const float ts = ImGui::GetFontSize() * 0.68f;
        float ty = by0 - ts * 1.35f;
        if (g_esp.drawDist) {
            const char* ds = "2.3 m";
            ImVec2 tw = f->CalcTextSizeA(ts, FLT_MAX, 0.0f, ds);
            dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f + 1.0f, ty + 1.0f), Md3U32(ImVec4(0, 0, 0, 0.70f)), ds);
            dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f, ty), Md3U32(g_esp.colText), ds);
            ty -= ts * 1.30f;
        }
        if (g_esp.drawId) {
            const char* is = "ID:22";
            ImVec2 tw = f->CalcTextSizeA(ts, FLT_MAX, 0.0f, is);
            dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f + 1.0f, ty + 1.0f), Md3U32(ImVec4(0, 0, 0, 0.70f)), is);
            dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f, ty), Md3U32(g_esp.colText), is);
        }
    }

    // ---- 骨骼 ----
    if (g_esp.drawSkeleton) {
        const ImU32 sk = Md3U32(g_esp.colSkel);
        const float st = g_esp.boxThick;
        dl->AddLine(head, neck, sk, st);
        dl->AddLine(neck, shL, sk, st);
        dl->AddLine(neck, shR, sk, st);
        dl->AddLine(shL, elL, sk, st);
        dl->AddLine(elL, haL, sk, st);
        dl->AddLine(shR, elR, sk, st);
        dl->AddLine(elR, haR, sk, st);
        dl->AddLine(neck, hip, sk, st);
        dl->AddLine(hip, knL, sk, st);
        dl->AddLine(knL, ftL, sk, st);
        dl->AddLine(hip, knR, sk, st);
        dl->AddLine(knR, ftR, sk, st);
        dl->AddCircle(head, u * 0.60f, sk, 20, st);

        ImVec2 joints[12] = { neck, shL, shR, elL, elR, haL, haR, hip, knL, knR, ftL, ftR };
        for (int i = 0; i < 12; ++i) dl->AddCircleFilled(joints[i], st * 1.7f, sk, 8);
    }

    // ---- 射线 (多层叠加 = 发光) ----
    if (g_esp.drawTracer) {
        const ImVec4& tv = g_esp.colTracer;
        const ImVec2 from(p0.x + w * 0.5f, p1.y - 3.0f);
        dl->AddLine(from, neck, Md3U32(ImVec4(tv.x, tv.y, tv.z, 0.20f)), 4.5f);
        dl->AddLine(from, neck, Md3U32(ImVec4(tv.x, tv.y, tv.z, 0.50f)), 2.4f);
        dl->AddLine(from, neck, Md3U32(tv), 1.2f);
    }

    dl->PopClipRect();
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(w, height));
}

// ---------------------------------------------------------------------
//  「绘制」页
// ---------------------------------------------------------------------
void PageDraw()
{
    const ImVec4& OnSurface            = CurPalette().OnSurface;
    const ImVec4& OnSurfaceVariant     = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& OnPrimaryContainer   = CurPalette().OnPrimaryContainer;

    PageHeader("绘制", "ESP 视觉绘制 · 开关、样式、配色与实时预览");

    // ---------- 2) 绘制项 ----------
    Md3Card("##card_esp_switch", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("绘制项");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // ---- 总开关：一键全开 / 全关 ----
        bool master = g_esp.enabled;
        if (Md3Checkbox("总开关（一键全开 / 全关）", &master)) {
            g_esp.enabled      = master;
            g_esp.drawId       = master;
            g_esp.drawDist     = master;
            g_esp.drawBox      = master;
            g_esp.drawSkeleton = master;
            g_esp.drawTracer   = master;
            g_esp.drawHealth   = master;
        }

        Md3Checkbox("绘制 ID", &g_esp.drawId);
        Md3Checkbox("显示距离", &g_esp.drawDist);
        Md3Checkbox("方框", &g_esp.drawBox);
        Md3Checkbox("骨骼", &g_esp.drawSkeleton);
        Md3Checkbox("射线", &g_esp.drawTracer);
        Md3Checkbox("血条", &g_esp.drawHealth);

    }
    Md3CardEnd();

    // ---------- 3) 方框样式 ----------
    Md3Card("##card_esp_style", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("样式");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        Md3RadioButton("2D 方框", &g_esp.boxStyle, 0);
        ImGui::SameLine();
        Md3RadioButton("3D 方框", &g_esp.boxStyle, 1);
        ImGui::SameLine();
        Md3RadioButton("霓虹框", &g_esp.boxStyle, 2);

        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("线宽", &g_esp.boxThick, 1.0f, 5.0f, "%.1f px");
    }
    Md3CardEnd();

    // ---------- 4) 绘图颜色 (ImGui 内置调色板) ----------
    Md3Card("##card_esp_color", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("绘图颜色");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        EspColorRow("方框颜色", g_esp.colBox, OnSurfaceVariant);
        EspColorRow("骨骼颜色", g_esp.colSkel, OnSurfaceVariant);
        EspColorRow("文字颜色", g_esp.colText, OnSurfaceVariant);
        EspColorRow("射线颜色", g_esp.colTracer, OnSurfaceVariant);

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted("点色块即打开 ImGui 自带调色板（色轮 / 取色器 / 透明度）");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    // ---------- 5) 范围 ----------
    Md3Card("##card_esp_range", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("范围");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("绘制距离", &g_esp.range, 10.0f, 200.0f, "%.0f m");
    }
    Md3CardEnd();
}

void PageTarget() {
    // ---- 二级导航：战斗 / 美化（在主导航下面，页内切换） ----
    static int s_subTab = 0;
    {
        static const char* kTabs[] = { "战斗", "美化" };
        Md3SubNav("##battle_subnav", kTabs, 2, &s_subTab);
    }
    if (s_subTab == 1) { PageBeautify(); return; }

    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;
    const ImVec4& Primary = CurPalette().Primary;

    PageHeader(T(SK_BATTLE_TITLE), T(SK_BATTLE_SUB));

    // ================= 自瞄范围圈（置顶：一进页面就能看见圆圈）=================
    // 只画一条细圆圈：无填充、无光晕、无描边叠加
    Md3Card("##card_combat_aimcircle", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("自瞄范围圈");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // ---- 预览画布（放最前，拉开页面就能看到圆圈）----
        {
            const float h = 150.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImVec2 p1(p0.x + w, p0.y + h);
            const ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);

            dl->PushClipRect(p0, p1, true);
            dl->AddRectFilled(p0, p1, Md3U32(ImVec4(0.07f, 0.08f, 0.10f, 1.0f)), 12.0f);
            dl->AddLine(ImVec2(c.x - 12.0f, c.y), ImVec2(c.x + 12.0f, c.y), Md3U32(ImVec4(1, 1, 1, 0.30f)), 1.0f);
            dl->AddLine(ImVec2(c.x, c.y - 12.0f), ImVec2(c.x, c.y + 12.0f), Md3U32(ImVec4(1, 1, 1, 0.30f)), 1.0f);
            ImVec4 cc = g_aimColor;
            if (!g_aimCircle) cc.w = 0.30f;
            // 自适应限幅：半径调到多大，整圈都完整可见（不会被裁掉）
            float shownR = g_aimRadius * 0.34f;
            const float maxR = h * 0.5f - 16.0f;
            if (shownR > maxR) shownR = maxR;
            dl->AddCircle(c, shownR, Md3U32(cc), 72, g_aimThick);
            dl->PopClipRect();

            ImGui::SetCursorScreenPos(p0);
            ImGui::Dummy(ImVec2(w, h));
        }

        Md3Checkbox("显示范围圈", &g_aimCircle);
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("范围半径", &g_aimRadius, 20.0f, 420.0f, "%.0f px");
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("线宽", &g_aimThick, 1.0f, 3.0f, "%.1f px");
        EspColorRow("圆圈颜色", g_aimColor, OnSurfaceVariant);

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted("仅一条细圆圈 · 无填充 · 无光晕 · 实时圆圈限制在屏幕内，永远完整可见");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    Md3Card("##card_combat_toggle", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_MASTER_SWITCH));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        Md3Checkbox(T(SK_ENABLE_BATTLE), &g_combatEnabled);
        Md3Checkbox(T(SK_AUTO_MODE), &g_combatAuto);
    }
    Md3CardEnd();

    Md3Card("##card_combat_params", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_PARAMS));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat(T(SK_ATK_RANGE), &g_combatRange, 1.0f, 20.0f, T(SK_ATK_RANGE_FMT));
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat(T(SK_ATK_SPEED), &g_combatSpeed, 0.5f, 10.0f, T(SK_ATK_SPEED_FMT));
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat(T(SK_VIEW_FOV), &g_combatFOV, 30.0f, 120.0f, "%.0f°");
        ImGui::SetNextItemWidth(-1.0f);
        Md3Combo("##combat_mode", &g_combatMode, T(SK_COMBO_MELEE));
    }
    Md3CardEnd();

    Md3Card("##card_combat_status", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_RUN_STATUS));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, g_combatEnabled ? Primary : OnSurfaceVariant);
        ImGui::Text(T(SK_MODULE_FMT), g_combatEnabled ? T(SK_RUNNING) : T(SK_STOPPED));
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::Text(T(SK_MODE_AUTO_FMT),
            g_combatMode == 0 ? "Melee" : (g_combatMode == 1 ? "Ranged" : "Hybrid"),
            g_combatAuto ? T(SK_YES) : T(SK_NO));
        ImGui::PopStyleColor();
    }
    Md3CardEnd();
}

void PageGlobe() {
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;
    const ImVec4& Primary = CurPalette().Primary;

    PageHeader(T(SK_WORLD_TITLE), T(SK_WORLD_SUB));

    Md3Card("##card_world_server", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_SERVER_CFG));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##addr", T(SK_SERVER_ADDR_HINT), g_worldAddr, sizeof(g_worldAddr));
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderInt("##port", &g_worldPort, 1, 65535, T(SK_PORT_FMT));
        ImGui::SetNextItemWidth(-1.0f);
        Md3Combo("##protocol", &g_worldProtocol, "TCP\0UDP\0RakNet\0\0");

        if (Md3Button(g_worldConnected ? T(SK_DISCONNECT) : T(SK_CONNECT), ImVec2(-1, 0))) {
            g_worldConnected = !g_worldConnected;
            PushLog(LOG_INFO, "%s: %s:%d", g_worldConnected ? T(SK_CONNECT) : T(SK_DISCONNECT), g_worldAddr, g_worldPort);
        }
    }
    Md3CardEnd();

    Md3Card("##card_world_status", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_CONN_STATUS));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, g_worldConnected ? Primary : OnSurfaceVariant);
        ImGui::Text(T(SK_STATUS_FMT), g_worldConnected ? T(SK_CONNECTED) : T(SK_NOT_CONNECTED));
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::Text(T(SK_ADDR_FMT), g_worldAddr, g_worldPort);
        ImGui::Text(T(SK_PROTOCOL_FMT), g_worldProtocol == 0 ? "TCP" : (g_worldProtocol == 1 ? "UDP" : "RakNet"));
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    Md3Card("##card_world_latency", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_LATENCY));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        if (g_worldConnected) {
            g_worldLatency = 20.0f + sinf((float)ImGui::GetTime() * 2.0f) * 10.0f;
            ImGui::SetNextItemWidth(-1.0f);
            Md3SliderFloat("##latency", &g_worldLatency, 0.0f, 200.0f, "%.0f ms");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
            ImGui::TextWrapped("%s", T(SK_NO_LATENCY));
            ImGui::PopStyleColor();
        }
    }
    Md3CardEnd();
}

void PageSettings() {
    const ImVec4& Primary = CurPalette().Primary;
    const ImVec4& OnSurfaceVariant = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;

    PageHeader(T(SK_SETTINGS_TITLE), T(SK_SETTINGS_SUB));

    // 工作区 & MCP（放在最上面，方便随时改）
    PageWorkspaceCard();

    Md3Card("##card_set_general", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_GENERAL));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        Md3Checkbox(T(SK_NOTIFY), &g_setNotify);
        Md3Checkbox(T(SK_AUTO_START), &g_setAutoStart);
        Md3Checkbox(T(SK_HAPTIC), &g_setHaptic);
        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("##brightness", &g_setBrightness, 0.2f, 1.0f, T(SK_BRIGHTNESS_FMT));
    }
    Md3CardEnd();

    Md3Card("##card_set_volume", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_VOLUME));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        Md3SliderFloat("##vol", &g_setVolume, 0.0f, 1.0f, T(SK_VOLUME_FMT));
    }
    Md3CardEnd();

    Md3Card("##card_set_appearance", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_APPEARANCE));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::TextUnformatted(T(SK_COLOR_SCHEME));
        int prevScheme = g_themeScheme;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##scheme", ThemeName(g_themeScheme))) {
            for (int i = 0; i < THEME_COUNT; i++) {
                bool sel = (i == g_themeScheme);
                if (ImGui::Selectable(ThemeName(i), sel)) g_themeScheme = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (prevScheme != g_themeScheme) {
            SetupMD3Theme();
            PushLog(LOG_INFO, "%s: %s", T(SK_COLOR_SCHEME), ThemeName(g_themeScheme));
        }
        ImGui::TextUnformatted(T(SK_DARK_LIGHT));
        int prevChoice = g_setChoice;
        ImGui::SetNextItemWidth(-1.0f);
        Md3Combo("##theme", &g_setChoice, T(SK_COMBO_THEME));
        if (prevChoice != g_setChoice) {
            g_darkMode = (g_setChoice != 1);
            SetupMD3Theme();
            PushLog(LOG_INFO, "%s: %s", T(SK_DARK_LIGHT), g_setChoice == 1 ? "Light" : "Dark");
        }
    }
    Md3CardEnd();

    // ---- 立绘语音 (TTS) ----
    Md3Card("##card_set_tts", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("立绘语音");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        bool ttsOn = Tts::Enabled();
        if (Md3Checkbox("AI 回复时语音播报", &ttsOn)) Tts::SetEnabled(ttsOn);

        bool loli = Tts::Loli();
        if (Md3Checkbox("萝莉音", &loli)) Tts::SetLoli(loli);

        ImGui::TextUnformatted("音调");
        float tp = Tts::Pitch();
        ImGui::SetNextItemWidth(-1.0f);
        if (Md3SliderFloat("##tts_pitch", &tp, 0.5f, 2.0f, "%.2f")) Tts::SetPitch(tp);

        ImGui::TextUnformatted("语速");
        float tr = Tts::Rate();
        ImGui::SetNextItemWidth(-1.0f);
        if (Md3SliderFloat("##tts_rate", &tr, 0.5f, 2.0f, "%.2f")) Tts::SetRate(tr);

        if (Md3Button("试听", ImVec2(120.0f, 38.0f)))
            Tts::Say("你好呀，我是你的小助手，请多关照～");
        ImGui::SameLine();
        bool tm = Live2D::TalkManual();
        if (Md3Checkbox("模拟说话", &tm)) Live2D::SetTalkManual(tm);

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted("AI 输出时逐句朗读，嘴巴同步开合；思考时立绘会摆头");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    // ---- 立绘 (Live2D Cubism) ----
    Md3Card("##card_set_live2d", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("立绘");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        bool l2dOn = Live2D::Show();
        if (Md3Checkbox("显示立绘", &l2dOn)) Live2D::SetShow(l2dOn);

        ImGui::TextUnformatted("模型缩放");
        float l2dScale = Live2D::Scale();
        ImGui::SetNextItemWidth(-1.0f);
        if (Md3SliderFloat("##l2d_scale_set", &l2dScale, 0.6f, 2.0f, "%.2f"))
            Live2D::SetScale(l2dScale);

        ImGui::TextUnformatted("头部跟随幅度");
        float l2dGain = Live2D::HeadGain();
        ImGui::SetNextItemWidth(-1.0f);
        if (Md3SliderFloat("##l2d_gain_set", &l2dGain, 0.0f, 60.0f, "%.0f°"))
            Live2D::SetHeadGain(l2dGain);

        bool l2dGaze = Live2D::GazeFollow();
        if (Md3Checkbox("触摸跟随视线", &l2dGaze)) Live2D::SetGazeFollow(l2dGaze);
        ImGui::SameLine();
        bool l2dBlink = Live2D::AutoBlink();
        if (Md3Checkbox("自动眨眼", &l2dBlink)) Live2D::SetAutoBlink(l2dBlink);

        if (Md3Button("怠速", ImVec2(120.0f, 38.0f))) Live2D::TriggerIdle();
        ImGui::SameLine();
        if (Md3Button("轻击", ImVec2(120.0f, 38.0f))) Live2D::TriggerTap();
        ImGui::SameLine();
        if (Md3Button("表情", ImVec2(120.0f, 38.0f))) Live2D::TriggerExpression();

        bool l2dTalk = Live2D::Talking();
        if (Md3Checkbox("模拟说话", &l2dTalk)) Live2D::SetTalking(l2dTalk);

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted("模型数据已内嵌在应用里，不依赖任何外部资源");
        ImGui::PushStyleColor(ImGuiCol_Text, CurPalette().Primary);
        ImGui::TextUnformatted(Live2D::StatusText());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    Md3Card("##card_set_fx", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(TR("特效与语言"));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        bool meteor = Fx::On();
        if (Md3Checkbox(TR("流星雨"), &meteor)) Fx::SetOn(meteor);

        // 语言: 一行切换, 全 UI 跟着变
        bool en = (g_setLang == 1);
        if (Md3Checkbox(TR("English"), &en)) {
            g_setLang = en ? 1 : 0;
            PushLog(LOG_INFO, "[语言] %s", en ? "English" : "中文");
        }
        ImGui::SameLine();
        bool dark = DarkModeOn();
        if (Md3Checkbox(TR("深度黑暗"), &dark)) SetDarkMode(dark);

        if (Md3Button(TR("立即恢复默认"), ImVec2(180.0f, 38.0f))) UiCmd::RestoreAll();

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted(TR("AI 拥有最高权限：回复里写 [[ui:key=val]] 即可操控本界面"));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    // ---- 运动模糊 ----
    Md3Card("##card_set_blur", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("运动模糊");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        bool mbOn = MotionBlur::Enabled();
        if (Md3Checkbox("启用运动模糊", &mbOn)) MotionBlur::SetEnabled(mbOn);

        float mbStrength = MotionBlur::Strength();
        ImGui::SetNextItemWidth(-1.0f);
        if (Md3SliderFloat("模糊力度", &mbStrength, 0.0f, 1.0f, "%.2f"))
            MotionBlur::SetStrength(mbStrength);

        ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextUnformatted("静止内容保持清晰 · 只有运动中的界面拖出残影 · 折叠时拉出拖尾");
        ImGui::PushStyleColor(ImGuiCol_Text, CurPalette().Primary);
        ImGui::TextUnformatted(MotionBlur::StatusText());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    Md3CardEnd();

    Md3Card("##card_set_lang", SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(T(SK_LANGUAGE));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        Md3Combo("##lang", &g_setLang, T(SK_COMBO_LANG));
    }
    Md3CardEnd();
}

// =====================================================================
//  包围盒来自模型每个 drawable 顶点的真实投影 (md3_live2d.cpp)：
//    · 方框永远框住角色本体 (不会超出, 也不会太小)
//    · 骨骼按真实包围盒比例 → 符合这个角色的身形
//    · 模型缩放 / 呼吸 / 动作时, 框与骨骼全程跟着动
// =====================================================================
static void EspPreviewStand(float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p1(p0.x + w, p0.y + height);
    dl->AddRectFilled(p0, p1, Md3U32(ImVec4(0.07f, 0.08f, 0.10f, 1.0f)), 12.0f);

    if (!Live2D::Ready()) {
        dl->AddText(ImVec2(p0.x + 12.0f, p0.y + height * 0.5f),
                    ImGui::GetColorU32(CurPalette().OnSurfaceVariant), Live2D::StatusText());
        return;
    }

    // 立绘纹理按正方形居中填进去
    const float side = (w < height) ? w : height;
    const ImVec2 q0(p0.x + (w - side) * 0.5f, p0.y + (height - side) * 0.5f);
    const ImVec2 q1(q0.x + side, q0.y + side);
    dl->AddImage((ImTextureID)(intptr_t)Live2D::Tex(), q0, q1, ImVec2(0, 1), ImVec2(1, 0));

    // 叠加: 直接画在真实角色上 (包围盒来自模型真实顶点投影)
    EspOverlayOnStand(dl, q0, q1);
}

static void EspOverlayOnStand(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    if (!(g_esp.drawBox || g_esp.drawSkeleton || g_esp.drawId || g_esp.drawDist)) return;
    if (b.x - a.x < 8.0f || b.y - a.y < 8.0f) return;

    // 真实包围盒 (归一化 0..1); 取不到就用保守默认值
    float nx0 = 0.36f, ny0 = 0.04f, nx1 = 0.64f, ny1 = 0.97f;
    Live2D::BoundsNorm(&nx0, &ny0, &nx1, &ny1);
    const float W = b.x - a.x, H = b.y - a.y;
    float x0 = a.x + nx0 * W, x1 = a.x + nx1 * W;
    float y0 = a.y + ny0 * H, y1 = a.y + ny1 * H;
    if (x1 - x0 < 10.0f) { x0 = a.x + W * 0.30f; x1 = a.x + W * 0.70f; }
    if (y1 - y0 < 10.0f) { y0 = a.y + H * 0.05f; y1 = a.y + H * 0.95f; }
    const float cx = (x0 + x1) * 0.5f;
    const float bw = x1 - x0, bh = y1 - y0;
    ImFont* f = ImGui::GetFont();
    const float ts = ImGui::GetFontSize() * 0.78f;
    const float is = ImGui::GetFontSize() * 0.84f;
    const float th = (g_esp.boxThick > 0.0f) ? g_esp.boxThick * 0.85f : 1.3f;

    // ---- 骨骼: 完全按真实包围盒比例 ----
    if (g_esp.drawSkeleton) {
        const ImU32 sk = Md3U32(g_esp.colSkel);
        const ImVec2 head(cx, y0 + bh * 0.105f);
        const ImVec2 neck(cx, y0 + bh * 0.215f);
        const ImVec2 shL(x0 + bw * 0.24f, neck.y + bh * 0.025f);
        const ImVec2 shR(x0 + bw * 0.76f, neck.y + bh * 0.025f);
        const ImVec2 elL(x0 + bw * 0.09f, y0 + bh * 0.395f);
        const ImVec2 elR(x0 + bw * 0.91f, y0 + bh * 0.395f);
        const ImVec2 haL(x0 + bw * 0.05f, y0 + bh * 0.565f);
        const ImVec2 haR(x0 + bw * 0.95f, y0 + bh * 0.565f);
        const ImVec2 hip(cx, y0 + bh * 0.545f);
        const ImVec2 knL(x0 + bw * 0.36f, y0 + bh * 0.765f);
        const ImVec2 knR(x0 + bw * 0.64f, y0 + bh * 0.765f);
        const ImVec2 ftL(x0 + bw * 0.33f, y1);
        const ImVec2 ftR(x0 + bw * 0.67f, y1);
        dl->AddCircle(head, bw * 0.155f, sk, 24, th);
        dl->AddLine(head, neck, sk, th);
        dl->AddLine(neck, shL, sk, th);   dl->AddLine(neck, shR, sk, th);
        dl->AddLine(shL, elL, sk, th);    dl->AddLine(elL, haL, sk, th);
        dl->AddLine(shR, elR, sk, th);    dl->AddLine(elR, haR, sk, th);
        dl->AddLine(neck, hip, sk, th);
        dl->AddLine(hip, knL, sk, th);    dl->AddLine(knL, ftL, sk, th);
        dl->AddLine(hip, knR, sk, th);    dl->AddLine(knR, ftR, sk, th);
    }

    // ---- 方框 (自动贴住真实包围盒) ----
    if (g_esp.drawBox) {
        const ImVec4& c = g_esp.colBox;
        if (g_esp.boxStyle == 1) {
            const float dx = bw * 0.12f, dy = bh * 0.05f;
            const ImU32 cc = Md3U32(c);
            dl->AddRect(ImVec2(x0 + dx, y0 - dy), ImVec2(x1 + dx, y1 - dy), cc, 0.0f, 0, g_esp.boxThick);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), cc, 0.0f, 0, g_esp.boxThick);
            dl->AddLine(ImVec2(x0, y0), ImVec2(x0 + dx, y0 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(x1, y0), ImVec2(x1 + dx, y0 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(x0, y1), ImVec2(x0 + dx, y1 - dy), cc, g_esp.boxThick);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x1 + dx, y1 - dy), cc, g_esp.boxThick);
        } else if (g_esp.boxStyle == 2) {
            dl->AddRect(ImVec2(x0 - 6, y0 - 6), ImVec2(x1 + 6, y1 + 6), Md3U32(ImVec4(c.x, c.y, c.z, 0.05f)), 6.0f, 0, 5.0f);
            dl->AddRect(ImVec2(x0 - 4, y0 - 4), ImVec2(x1 + 4, y1 + 4), Md3U32(ImVec4(c.x, c.y, c.z, 0.10f)), 5.0f, 0, 4.0f);
            dl->AddRect(ImVec2(x0 - 2, y0 - 2), ImVec2(x1 + 2, y1 + 2), Md3U32(ImVec4(c.x, c.y, c.z, 0.28f)), 4.0f, 0, 3.0f);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), Md3U32(c), 3.0f, 0, 1.6f);
        } else {
            dl->AddRect(ImVec2(x0 - 1.5f, y0 - 1.5f), ImVec2(x1 + 1.5f, y1 + 1.5f),
                        Md3U32(ImVec4(0, 0, 0, 0.55f)), 2.0f, 0, g_esp.boxThick + 2.0f);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), Md3U32(c), 2.0f, 0, g_esp.boxThick);
        }

        if (g_esp.drawHealth) {
            dl->AddRectFilled(ImVec2(x0 - 9.0f, y0), ImVec2(x0 - 4.0f, y1), Md3U32(ImVec4(0, 0, 0, 0.55f)), 3.0f);
            dl->AddRectFilled(ImVec2(x0 - 8.0f, y0 + bh * 0.30f), ImVec2(x0 - 5.0f, y1 - 1.0f),
                              Md3U32(ImVec4(0.30f, 0.90f, 0.45f, 1.0f)), 3.0f);
        }
    }

    // ---- ID 在头顶 / 距离在脚下 ----
    if (g_esp.drawId) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s : 22", TR("棍母"));
        const ImVec2 tw = f->CalcTextSizeA(is, FLT_MAX, 0.0f, buf);
        dl->AddText(f, is, ImVec2(cx - tw.x * 0.5f + 1.0f, y0 - is - 3.0f),
                    Md3U32(ImVec4(0, 0, 0, 0.7f)), buf);
        dl->AddText(f, is, ImVec2(cx - tw.x * 0.5f, y0 - is - 4.0f), Md3U32(g_esp.colText), buf);
    }
    if (g_esp.drawDist) {
        char buf[32];
        //   参考: 画布高 500px 时约 2.4m; 画布越小 -> 越远
        {
            static float s_distNow = 2.4f;
            const float ch = b.y - a.y;
            float dm = 2.4f * (500.0f / (ch > 1.0f ? ch : 1.0f));
            if (dm < 0.6f) dm = 0.6f;
            if (dm > g_esp.range) dm = g_esp.range;
            s_distNow += (dm - s_distNow) * 0.15f;      // 平滑, 不跳数
            snprintf(buf, sizeof(buf), "%.1f m", s_distNow);
        }
        const ImVec2 tw = f->CalcTextSizeA(ts, FLT_MAX, 0.0f, buf);
        dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f + 1.0f, y1 + 4.0f),
                    Md3U32(ImVec4(0, 0, 0, 0.7f)), buf);
        dl->AddText(f, ts, ImVec2(cx - tw.x * 0.5f, y1 + 3.0f), Md3U32(g_esp.colText), buf);
    }
}

static void EspPreviewStand(float height);


void SetNavIndex(int i) { g_navIndex = i; }
int  NavIndexOf() { return g_navIndex; }
bool EspMaster() { return g_esp.drawId && g_esp.drawDist && g_esp.drawBox && g_esp.drawSkeleton; }
void EspSetMaster(bool on) {
    g_esp.drawId = on; g_esp.drawDist = on; g_esp.drawBox = on;
    g_esp.drawSkeleton = on; g_esp.drawHealth = on;
}
void SetDarkMode(bool dark) { g_darkMode = dark; g_setChoice = dark ? 0 : 1; SetupMD3Theme(); }
bool DarkModeOn() { return g_darkMode; }

void RenderDemoFrame() {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);

    const ImVec4& Primary             = CurPalette().Primary;
    const ImVec4& Secondary            = CurPalette().Secondary;
    const ImVec4& Tertiary             = CurPalette().Tertiary;
    const ImVec4& PrimaryContainer     = CurPalette().PrimaryContainer;
    const ImVec4& OnPrimaryContainer   = CurPalette().OnPrimaryContainer;
    const ImVec4& OnSurface            = CurPalette().OnSurface;
    const ImVec4& OnSurfaceVariant     = CurPalette().OnSurfaceVariant;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& SurfaceContainer     = CurPalette().SurfaceContainer;
    const ImVec4& OutlineVariant       = CurPalette().OutlineVariant;

    // 平时用 FirstUseEver -> 可以正常拖动; 只在 ImGui 上下文刚重建的那两帧强恢复;
    // 大退(进程被杀)时静态变量归零 -> 回默认位置
    static float s_savedWX = 0.0f, s_savedWY = 0.0f;
    static float s_savedWW = 980.0f, s_savedWH = 876.0f;
    static bool  s_savedOK = false;
    const bool freshCtx = (ImGui::GetFrameCount() <= 2);
    if (!s_savedOK) {
        ImGui::SetNextWindowSize(ImVec2(980, 876), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 662, io.DisplaySize.y * 0.5f - 438), ImGuiCond_Always);
    } else if (freshCtx) {
        ImGui::SetNextWindowSize(ImVec2(s_savedWW, s_savedWH), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(s_savedWX, s_savedWY), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(ImVec2(s_savedWW, s_savedWH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(s_savedWX, s_savedWY), ImGuiCond_FirstUseEver);
    }
        // 主窗口实际矩形（供右侧「绘制预览」/立绘面板贴边定位）
    static float s_avX = 0.0f, s_avY = 0.0f, s_avW = 980.0f, s_avH = 876.0f;
    // 注意: 这里刻意不加 ImGuiWindowFlags_NoResize —— 右下角的缩放把柄是原版功能, 要保留。
    // 加 NoMove 是为了让内容区拖动不再误移整个窗口; 移动改由顶部应用栏拖拽负责。
    ImGui::Begin("Avates", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        s_avX = winPos.x; s_avY = winPos.y; s_avW = winSize.x; s_avH = winSize.y;
        s_savedWX = winPos.x; s_savedWY = winPos.y;
        s_savedWW = winSize.x; s_savedWH = winSize.y;
        s_savedOK = true;
        const float barH = 64.0f;

        dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + barH),
            Md3U32(SurfaceContainerHigh), 28.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(
            ImVec2(winPos.x, winPos.y + barH - 1),
            ImVec2(winPos.x + winSize.x, winPos.y + barH - 1),
            Md3U32(OutlineVariant), 1.0f);

        // ---- 顶部应用栏拖拽（唯一可以移动整个窗口的地方）----
        // 窗口带 ImGuiWindowFlags_NoMove，内容区再也不会被误拖走；
        // 这里用 SetWindowPos 手工实现，只认标题栏这一条热区。
        {
            ImGui::SetCursorScreenPos(winPos);
            ImGui::PushID(9001);
            ImGui::InvisibleButton("##bar_drag", ImVec2(winSize.x - 72.0f, barH));
            bool barHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
                ImVec2 d = ImGui::GetIO().MouseDelta;
                ImVec2 wp = ImGui::GetWindowPos();
                ImVec2 ns(wp.x + d.x, wp.y + d.y);
                // 限制在屏幕内，避免把窗口拖丢
                ImVec2 ds = ImGui::GetIO().DisplaySize;
                if (ns.x < -(winSize.x - 160.0f)) ns.x = -(winSize.x - 160.0f);
                if (ns.y < 0.0f) ns.y = 0.0f;
                if (ns.x > ds.x - 160.0f) ns.x = ds.x - 160.0f;
                if (ns.y > ds.y - 90.0f) ns.y = ds.y - 90.0f;
                ImGui::SetWindowPos(ns);
            }
            if (barHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            ImGui::PopID();
        }

        ImGui::SetCursorScreenPos(ImVec2(winPos.x + 24.0f, winPos.y + 18.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, OnSurface);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextUnformatted(NavLabel(g_navIndex));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        {
            float dotR = 5.0f;
            float dotGap = 14.0f;
            float dotsW = 3 * dotR * 2 + 2 * dotGap;
            float dotStartX = winPos.x + winSize.x - 24.0f - dotsW - 48.0f;
            float dotY = winPos.y + barH * 0.5f;
            dl->AddCircleFilled(ImVec2(dotStartX, dotY), dotR, Md3U32(Primary));
            dl->AddCircleFilled(ImVec2(dotStartX + dotR * 2 + dotGap, dotY), dotR, Md3U32(Secondary));
            dl->AddCircleFilled(ImVec2(dotStartX + (dotR * 2 + dotGap) * 2, dotY), dotR, Md3U32(Tertiary));

            // (音量键方案: 标题栏不参与收起判定)
        }

        {
            float btnR = 16.0f;
            ImVec2 btnCenter(winPos.x + winSize.x - 24.0f - btnR, winPos.y + barH * 0.5f);
            dl->AddCircle(btnCenter, btnR, Md3U32(OnSurfaceVariant), 32, 1.5f);
            if (g_darkMode) {
                dl->AddCircleFilled(btnCenter, btnR * 0.6f, Md3U32(OnSurfaceVariant));
                dl->AddCircleFilled(ImVec2(btnCenter.x + btnR * 0.25f, btnCenter.y - btnR * 0.15f),
                    btnR * 0.55f, Md3U32(SurfaceContainerHigh));
            } else {
                dl->AddCircleFilled(btnCenter, btnR * 0.4f, Md3U32(OnSurfaceVariant));
                for (int i = 0; i < 8; i++) {
                    float a = (float)i / 8.0f * 6.2831853f;
                    ImVec2 p1(btnCenter.x + cosf(a) * btnR * 0.55f,
                              btnCenter.y + sinf(a) * btnR * 0.55f);
                    ImVec2 p2(btnCenter.x + cosf(a) * btnR * 0.8f,
                              btnCenter.y + sinf(a) * btnR * 0.8f);
                    dl->AddLine(p1, p2, Md3U32(OnSurfaceVariant), 1.5f);
                }
            }
            ImGui::SetCursorScreenPos(ImVec2(btnCenter.x - btnR, btnCenter.y - btnR));
            ImGui::PushID(999);
            if (ImGui::InvisibleButton("##theme_toggle", ImVec2(btnR * 2, btnR * 2))) {
                g_darkMode = !g_darkMode;
                g_setChoice = g_darkMode ? 2 : 1;
                SetupMD3Theme();
                PushLog(LOG_INFO, "%s: %s", T(SK_DARK_LIGHT), g_darkMode ? "Dark" : "Light");
            }
            ImGui::PopID();
        }

        ImGui::SetCursorScreenPos(ImVec2(winPos.x, winPos.y + barH));
        ImGui::Dummy(ImVec2(0, 0));
    }

    const float railW = 120.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, SurfaceContainerHigh);
    ImGui::BeginChild("nav_rail", ImVec2(railW, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 railOrigin = ImGui::GetCursorScreenPos();

        // 导航项自适应：项目数(NAV_COUNT)变化时自动压缩，避免溢出
        float itemH = 120.0f;
        {
            float railH = ImGui::GetContentRegionAvail().y;
            if ((float)NAV_COUNT * itemH > railH - 8.0f)
                itemH = (railH - 8.0f) / (float)NAV_COUNT;
        }
        const float iconR = 20.0f;
        const float iconY = 38.0f;

        float targetY = railOrigin.y + g_navIndex * itemH;
        if (!g_indicatorInit) {
            g_indicatorY = targetY;
            g_indicatorInit = true;
        } else {
            SpringUpdate(g_indicatorY, g_indicatorVelY, targetY, dt);
        }

        {
            ImVec2 indMin(railOrigin.x + 8.0f, g_indicatorY + 8.0f);
            ImVec2 indMax(railOrigin.x + railW - 8.0f, g_indicatorY + itemH - 8.0f);
            dl->AddRectFilled(indMin, indMax, Md3U32(PrimaryContainer), 20.0f);
        }

        for (int i = 0; i < NAV_COUNT; i++) {
            ImVec2 itemMin(railOrigin.x, railOrigin.y + i * itemH);
            ImVec2 iconCenter(itemMin.x + railW * 0.5f, itemMin.y + iconY);
            bool selected = (i == g_navIndex);

            float iconScale = 1.0f;
            if (selected) {
                float bounceT = EaseOutBack(g_itemSwitchAnim);
                iconScale = 0.85f + 0.15f * bounceT;
            }

            bool hovered = false;
            if (!selected) {
                ImVec2 hoverMin(itemMin.x, itemMin.y);
                ImVec2 hoverMax(itemMin.x + railW, itemMin.y + itemH);
                hovered = ImGui::IsMouseHoveringRect(hoverMin, hoverMax);
                if (hovered) {
                    ImVec4 sl = OnSurfaceVariant;
                    sl.w = 0.08f;
                    dl->AddCircleFilled(iconCenter, iconR + 8.0f, Md3U32(sl), 24);
                }
            }

            ImU32 iconCol = Md3U32(selected ? OnPrimaryContainer : OnSurfaceVariant);
            float scaledR = iconR * iconScale;
            switch (i) {
            case NAV_HOME:     DrawIconHome(dl, iconCenter, scaledR, iconCol, selected); break;
            case NAV_TARGET:   DrawIconTarget(dl, iconCenter, scaledR, iconCol, selected); break;
            case NAV_DRAW:     DrawIconDraw(dl, iconCenter, scaledR, iconCol, selected); break;
            case NAV_GLOBE:    DrawIconGlobe(dl, iconCenter, scaledR, iconCol, selected); break;
            case NAV_TERMINAL: DrawIconTerminal(dl, iconCenter, scaledR, iconCol, selected); break;
            case NAV_SETTINGS: DrawIconSettings(dl, iconCenter, scaledR, iconCol, selected); break;
            }

            ImVec2 textSize = ImGui::CalcTextSize(NavLabel(i));
            ImVec2 textPos(iconCenter.x - textSize.x * 0.5f, iconCenter.y + iconR + 12.0f);
            dl->AddText(textPos, iconCol, NavLabel(i));

            if (i < NAV_COUNT - 1) {
                ImVec2 sepMin(itemMin.x + 16.0f, itemMin.y + itemH);
                ImVec2 sepMax(itemMin.x + railW - 16.0f, itemMin.y + itemH);
                dl->AddLine(sepMin, sepMax, Md3U32(OutlineVariant), 1.0f);
            }

            ImGui::SetCursorScreenPos(itemMin);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##nav", ImVec2(railW, itemH))) {
                if (g_navIndex != i) {
                    g_navIndex = i;
                    g_itemSwitchAnim = 0.0f;
                }
            }
            ImGui::PopID();
        }

        g_itemSwitchAnim += dt * 2.0f;
        if (g_itemSwitchAnim > 1.0f) g_itemSwitchAnim = 1.0f;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    float animT = EaseOutBack(g_itemSwitchAnim);
    if (animT < 0.0f) animT = 0.0f;
    if (animT > 1.0f) animT = 1.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, animT);
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    contentPos.y += (1.0f - animT) * 24.0f;
    ImGui::SetCursorScreenPos(contentPos);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, SurfaceContainer);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    ImGui::BeginChild("content", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PopStyleVar();
    g_cardCounter = 0;
    if (!Lic::Unlocked()) {
        Lic::DrawPage();
        HandleTouchDragScroll(true);
    } else if (g_navIndex == NAV_HOME) {
        float availH = ImGui::GetContentRegionAvail().y;
        float panelH = availH * 0.54f;
        if (panelH < 260.0f) panelH = 260.0f;
        if (panelH > availH - 130.0f) panelH = availH - 130.0f;
        if (panelH < 140.0f) panelH = 140.0f;
        float topH = availH - panelH - ImGui::GetStyle().ItemSpacing.y;
        if (topH < 60.0f) topH = 60.0f;

        ImGui::BeginChild("home_scroll", ImVec2(0, topH), ImGuiChildFlags_None,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        PageHome();
        HandleTouchDragScroll(true);
        ImGui::EndChild();

        ImGui::BeginChild("home_ai", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        PageAiChat();
        ImGui::EndChild();
    } else {
        switch (g_navIndex) {
        case NAV_TARGET:   PageTarget(); break;
        case NAV_DRAW:     PageDraw(); break;
        case NAV_GLOBE:    PageGlobe(); break;
        case NAV_TERMINAL: PageTerminal(); break;
        case NAV_SETTINGS: PageSettings(); break;
        default: break;
        }
        HandleTouchDragScroll(true);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::End();

    // =================================================================
    //  右侧「绘制预览」独立面板
    //  * 贴在主 UI 右边、留间隔
    //  * 只在「绘制」导航页显示（点其他导航立即消失）
    //  * 音量减折叠时，本面板随整帧顶点一起收进去
    // =================================================================
    if (g_navIndex == NAV_DRAW) {
        const float pw  = 320.0f;
        const float gap = 14.0f;
        float px = s_avX + s_avW + gap;
        float py = s_avY;
        float ph = s_avH;

        // 超出屏幕右边缘就收回来，保证整块面板永远完整可见
        if (px + pw > io.DisplaySize.x - 8.0f) px = io.DisplaySize.x - 8.0f - pw;
        if (px < 8.0f) px = 8.0f;
        if (py + ph > io.DisplaySize.y - 8.0f) ph = io.DisplaySize.y - 8.0f - py;
        if (ph < 200.0f) ph = 200.0f;

        ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, SurfaceContainerHigh);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::Begin("##esp_preview_panel", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, OnPrimaryContainer);
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextUnformatted("绘制预览");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            float canvasH = ImGui::GetContentRegionAvail().y - 30.0f;
            if (canvasH < 140.0f) canvasH = 140.0f;
            EspPreviewStand(canvasH);

            ImGui::PushStyleColor(ImGuiCol_Text, OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.70f);
            ImGui::TextUnformatted("仅「绘制」页显示 · 随界面折叠");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    // =================================================================
    //  立绘 (Live2D Cubism) —— 方案照 ImGuiLive2d-main:
    //    模型数据编译进 so / 离屏 FBO / 当纹理贴进独立小窗
    //  * 默认贴主 UI 右边 (间隔 14) · 与内容区上下平齐
    //  * 「绘制」页右侧已有预览面板时自动改贴左边, 永不重叠
    //  * 音量减折叠同样会把它一起收进去 (作用在整帧顶点上)
    // =================================================================
    if (Live2D::Show()) {
        float lpw = s_avH;                 // 正方形面板: 宽=高 → 画布能占满
        if (lpw < 300.0f) lpw = 300.0f;
        if (lpw > io.DisplaySize.x * 0.5f - 40.0f) lpw = io.DisplaySize.x * 0.5f - 40.0f;   // 只在屏幕真放不下时才压窄, 否则画布填满面板
        const float lgap = 14.0f;
        const bool  previewOn = (g_navIndex == NAV_DRAW);
        float lpx = previewOn ? (s_avX - lgap - lpw) : (s_avX + s_avW + lgap);
        float lpy = s_avY;
        float lph = s_avH;

        if (lpx + lpw > io.DisplaySize.x - 8.0f) lpx = io.DisplaySize.x - 8.0f - lpw;
        if (lpx < 8.0f) lpx = 8.0f;
        if (lpy + lph > io.DisplaySize.y - 8.0f) lph = io.DisplaySize.y - 8.0f - lpy;
        if (lph < 280.0f) lph = 280.0f;

        Live2D::DrawStandPanel(lpx, lpy, lpw, lph);

        ImVec2 ca, cb;
        Live2D::LastCanvasRect(&ca, &cb);
        if (cb.x > ca.x + 4.0f)
            EspOverlayOnStand(ImGui::GetForegroundDrawList(), ca, cb);
    }

    // =================================================================
    //  自瞄范围圈 · 真·实时的整屏 overlay
    //  画在 ImGui 前景层（最上层，压在任何窗口/面板之上）
    // =================================================================
    if (g_aimCircle) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 center(ds.x * 0.5f, ds.y * 0.5f);

        // 半径自动收进屏幕边界：半径调多大，整圈都完整可见
        float r = g_aimRadius;
        const float maxR = (ds.x < ds.y ? ds.x : ds.y) * 0.5f - 12.0f;
        if (r > maxR) r = maxR;

        fg->AddCircle(center, r, Md3U32(g_aimColor), 96, g_aimThick);
    }

    {
        ImDrawList* fg2 = ImGui::GetForegroundDrawList();
        Fx::Draw(fg2, io.DisplaySize.x, io.DisplaySize.y);
        // 卡密错误: 全屏红光一闪
        const float e = Lic::ErrAmount();
        if (e > 0.001f) {
            fg2->AddRectFilled(ImVec2(0, 0), io.DisplaySize,
                                IM_COL32(255, 30, 30, (int)(e * 70)), 0.0f);
            fg2->AddRect(ImVec2(2, 2), ImVec2(io.DisplaySize.x - 2, io.DisplaySize.y - 2),
                         IM_COL32(255, 60, 60, (int)(e * 220)), 0.0f, 0, 6.0f);
        }
    }

    // =================================================================
    // =================================================================
    {
        const bool iconOn = false;
        if (iconOn) {
            const float IS = 64.0f;
            if (g_iconX < 0.0f) {
                g_iconX = io.DisplaySize.x - IS - 26.0f;
                g_iconY = io.DisplaySize.y * 0.5f - IS * 0.5f;
            }
            {
                float ndx = 0.0f, ndy = 0.0f;
                UiAnim::TakeNudge(ndx, ndy);
                if (ndx != 0.0f || ndy != 0.0f) {
                    g_iconX += ndx; g_iconY += ndy;
                }
                if (g_iconX < 0.0f) g_iconX = 0.0f;
                if (g_iconY < 0.0f) g_iconY = 0.0f;
                if (g_iconX > io.DisplaySize.x - IS) g_iconX = io.DisplaySize.x - IS;
                if (g_iconY > io.DisplaySize.y - IS) g_iconY = io.DisplaySize.y - IS;
                if (UiAnim::TakeRequestShow()) UiAnim::IconShow(g_iconX + IS * 0.5f, g_iconY + IS * 0.5f);
                UiAnim::SetIconAnchor(g_iconX + IS * 0.5f, g_iconY + IS * 0.5f);
                UiAnim::SetIconHit(g_iconX, g_iconY, g_iconX + IS, g_iconY + IS);
            }

            UiIcon::EnsureTexture();
            ImDrawList* fg4 = ImGui::GetForegroundDrawList();
            const int iv0 = fg4->VtxBuffer.Size;
            const ImVec2 ic0(g_iconX, g_iconY);
            const ImVec2 ic1(g_iconX + IS, g_iconY + IS);
            const ImVec2 icc(g_iconX + IS * 0.5f, g_iconY + IS * 0.5f);
            float ia = 1.0f;
            if (UiAnim::IconAnimShowing()) ia = 1.0f - UiAnim::IconAnimProgress();
            if (ia < 0.0f) ia = 0.0f;
            const int iaB = (int)(ia * 255.0f);
            fg4->AddCircleFilled(icc, IS * 0.5f, IM_COL32(255, 255, 255, iaB), 48);
            const unsigned int icTex = UiIcon::Texture();
            if (icTex != 0)
                fg4->AddImageRounded((ImTextureID)(intptr_t)icTex, ic0, ic1, ImVec2(0, 0), ImVec2(1, 1),
                                     IM_COL32(255, 255, 255, iaB), IS * 0.5f, ImDrawFlags_RoundCornersAll);
            fg4->AddCircle(icc, IS * 0.5f, IM_COL32(0, 0, 0, (int)(46.0f * ia)), 48, 1.5f);
            const int iv1 = fg4->VtxBuffer.Size;
            UiAnim::SetKeepVerts(1, (const void*)fg4, iv0, iv1);
        } else {
            UiAnim::ClearKeepVerts(1);
            UiAnim::SetIconHit(0.0f, 0.0f, -1.0f, -1.0f);   // 图标不可见 -> 命中区关闭
        }
    }
}
