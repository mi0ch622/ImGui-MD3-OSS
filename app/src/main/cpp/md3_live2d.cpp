// =====================================================================
// md3_live2d.cpp - 立绘 (Live2D Cubism) 集成
// ---------------------------------------------------------------------
// 实现路线完全照 ImGuiLive2d-main:
//   ① 模型数据(moc3/贴图/动作/物理)全部编译进 so —— 不需要 assets 目录
//   ② 模型渲染到离屏 FBO (2048x2048, 透明底)
//   ③ FBO 纹理直接丢给 ImGui::Image 贴进窗口
//   ④ 渲染前后快照/恢复 GL 状态, 不污染 ImGui 自己的渲染管线
//
// 交互(照它 README):
//   触摸点视线跟随(头慢眼快, 指数缓动) / 自动眨眼 / 呼吸起伏 /
//   模拟说话 / 短按轻击体运动 / 随机表情 / 实时调参
// =====================================================================
#include "md3_common.h"
#include "Live2DManager.hpp"
#include "Live2DModel.hpp"
#include <Math/CubismMatrix44.hpp>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cmath>

namespace Live2D {

// ---------------- 状态 ----------------
static Live2DManager* s_mgr   = nullptr;
static GLuint s_fbo = 0, s_tex = 0;
static int    s_size  = 0;          // 离屏分辨率(正方形)
static bool   s_inited = false;
static bool   s_failed = false;
static bool   s_needRebuild = false;

static bool   s_show      = true;
static float  s_scale      = 1.35f;
static float  s_headGain   = 30.0f;  // 头部幅度(度)
static bool   s_gazeFollow = true;
static bool   s_autoBlink  = true;
static bool   s_talking    = false;
static bool   s_talkManual = false;   // 设置里的手动“模拟说话”
static bool   s_thinking   = false;
static float  s_thinkPhase = 0.0f;
static float  s_talkPhase  = 0.0f;

static float  s_tgtX = 0.0f, s_tgtY = 0.0f;   // 视线目标(-1..1)
static float  s_curX = 0.0f, s_curY = 0.0f;   // 缓动后
static bool   s_hovering = false;

static char   s_status[192] = "未初始化";

// 角色在画布里的真实包围盒 (归一化 0..1, y 向下) —— 绘制/ESP 全靠它贴合
static float  s_bMinX = 0.36f, s_bMinY = 0.04f, s_bMaxX = 0.64f, s_bMaxY = 0.97f;
static bool   s_boundsOk = false;
static ImVec2 s_canvasA(0, 0), s_canvasB(0, 0);   // 立绘画布在屏幕上的矩形

// ---------------- 离屏 FBO ----------------
static void DropFbo() {
    // 上下文可能已销毁: 只清句柄, 不调 glDelete
    s_fbo = 0;
    s_tex = 0;
    s_size = 0;
}

static bool EnsureFbo(int size) {
    if (s_tex && s_fbo && s_size == size) return true;
    DropFbo();

    glGenTextures(1, &s_tex);
    glBindTexture(GL_TEXTURE_2D, s_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_tex, 0);
    const GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (st != GL_FRAMEBUFFER_COMPLETE) {
        snprintf(s_status, sizeof(s_status), "离屏FBO不完整 0x%04X", (unsigned)st);
        DropFbo();
        return false;
    }
    s_size = size;
    return true;
}

// ---------------- 初始化 / 重建 ----------------
static bool EnsureInit() {
    if (s_inited) return true;
    if (s_failed) return false;

    s_mgr = &Live2DManager::GetInstance();
    s_mgr->Init();
    if (!s_mgr->IsInitialized()) {
        s_failed = true;
        snprintf(s_status, sizeof(s_status), "CubismFramework 启动失败");
        return false;
    }

    // 模型目录/文件 = 内嵌数组的名字 (live2d/LAppPal.cpp 内嵌查表)
    if (!s_mgr->LoadModel("Hiyori", "Hiyori.model3.json")) {
        s_failed = true;
        snprintf(s_status, sizeof(s_status), "模型加载失败(内嵌数据查表未命中)");
        return false;
    }

    Live2DModel* m = s_mgr->GetModel();
    if (m) {
        m->PreloadMotionGroup("Idle");
        m->PreloadMotionGroup("TapBody");
        m->StartRandomMotion("Idle", 1);   // PriorityIdle = 1
    }

    s_inited = true;
    snprintf(s_status, sizeof(s_status), "就绪 · 内嵌 Hiyori · 动作/物理/眨眼已启用");
    return true;
}

/** 上下文丢失 / 首次进入时重建整条链路 */
static void RebuildNow() {
    DropFbo();
    if (s_inited) {
        Live2DManager::DeleteInstance();
        s_mgr = nullptr;
        s_inited = false;
    }
    s_failed = false;
    s_curX = s_curY = s_tgtX = s_tgtY = 0.0f;
}

// ---------------- 每帧参数 ----------------
static void ApplyParameters() {
    Live2DModel* m = s_mgr ? s_mgr->GetModel() : nullptr;
    if (!m) return;

    // 视线缓动: 头慢(0.10) 眼快(0.22)
    const float kHead = 0.10f, kEye = 0.22f;
    s_curX += (s_tgtX - s_curX) * kEye;
    s_curY += (s_tgtY - s_curY) * kEye;
    const float hx = s_curX;   // 头用更慢的一路(近似)
    const float hy = s_curY;

    m->SetParameterValue("ParamAngleX",  hx * s_headGain);
    m->SetParameterValue("ParamAngleY", -hy * s_headGain * 0.6f);
    m->SetParameterValue("ParamAngleZ",  hx * s_headGain * 0.2f);
    m->SetParameterValue("ParamEyeBallX", s_curX);
    m->SetParameterValue("ParamEyeBallY", -s_curY);
    m->SetParameterValue("ParamBodyAngleX", hx * 6.0f);

    if (!s_gazeFollow) {
        m->SetParameterValue("ParamAngleX", 0.0f);
        m->SetParameterValue("ParamAngleY", 0.0f);
        m->SetParameterValue("ParamAngleZ", 0.0f);
        m->SetParameterValue("ParamEyeBallX", 0.0f);
        m->SetParameterValue("ParamEyeBallY", 0.0f);
        m->SetParameterValue("ParamBodyAngleX", 0.0f);
    }

    if (s_thinking) {
        s_thinkPhase += 0.035f;
        if (s_thinkPhase > 6.28318f) s_thinkPhase -= 6.28318f;
        m->SetParameterValue("ParamAngleZ", sinf(s_thinkPhase) * 7.0f);
        m->SetParameterValue("ParamAngleY", -4.0f);
        m->SetParameterValue("ParamBodyAngleZ", sinf(s_thinkPhase) * 4.0f);
    }

    // 模拟说话: 嘴巴随机开合
    if (s_talking) {
        s_talkPhase += 0.30f;
        if (s_talkPhase > 6.28318f) s_talkPhase -= 6.28318f;
        const float v = 0.35f + 0.65f * (0.5f + 0.5f * sinf(s_talkPhase * 2.3f));
        m->SetParameterValue("ParamMouthOpenY", v);
        m->SetParameterValue("ParamMouthForm", 0.6f);
    } else if (!s_autoBlink) {
        // 不用额外动作, 仅保证嘴巴闭上
        m->SetParameterValue("ParamMouthOpenY", 0.0f);
    }

    if (!s_autoBlink) {
        m->SetParameterValue("ParamEyeLOpen", 1.0f);
        m->SetParameterValue("ParamEyeROpen", 1.0f);
    }
}

// ---------------- 离屏渲染 ----------------
static void RenderToTexture() {
    if (!s_mgr || !s_tex || !s_fbo) return;

    // ① 快照 GL 状态 (Live2D 渲染器会改这些)
    GLint    prevFbo = 0, prevVp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_VIEWPORT, prevVp);
    const GLboolean bBlend = glIsEnabled(GL_BLEND);
    const GLboolean bDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean bCull  = glIsEnabled(GL_CULL_FACE);
    const GLboolean bSciss = glIsEnabled(GL_SCISSOR_TEST);

    // ② 渲染模型到离屏 FBO (透明底)
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glViewport(0, 0, s_size, s_size);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Csm::CubismMatrix44 proj;
    proj.Scale(s_scale, s_scale);
    s_mgr->Draw(proj);          // Draw 内部会 proj *= modelMatrix → 之后 proj 就是 MVP

    // ---- 用同一套 MVP 把模型所有可见 drawable 顶点投回画布，算出真实包围盒 ----
    {
        s_boundsOk = false;
        Live2DModel* lm = s_mgr->GetModel();
        Csm::CubismModel* cm = lm ? lm->GetModel() : nullptr;
        if (cm && cm->GetDrawableCount() > 0) {
            float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
            const int dc = cm->GetDrawableCount();
            for (int i = 0; i < dc; ++i) {
                if (!cm->GetDrawableDynamicFlagIsVisible(i)) continue;
                const int vc = cm->GetDrawableVertexCount(i);
                const Csm::csmFloat32* v = cm->GetDrawableVertices(i);
                if (!v) continue;
                const Csm::csmFloat32* mm = proj.GetArray();
                for (int j = 0; j < vc; ++j) {
                    const float x = v[j * 2 + 0], y = v[j * 2 + 1];
                    // 手写 MVP 变换 (列主序: m[12]/m[13] 是平移)
                    const float sx = mm[0] * x + mm[4] * y + mm[12];
                    const float sy = mm[1] * x + mm[5] * y + mm[13];
                    if (sx < mnx) mnx = sx;
                    if (sx > mxx) mxx = sx;
                    if (sy < mny) mny = sy;
                    if (sy > mxy) mxy = sy;
                }
            }
            if (mxx > mnx && mxy > mny) {
                // NDC → 归一化 (0..1, y 向下)
                s_bMinX = mnx * 0.5f + 0.5f;
                s_bMaxX = mxx * 0.5f + 0.5f;
                s_bMinY = 1.0f - (mxy * 0.5f + 0.5f);
                s_bMaxY = 1.0f - (mny * 0.5f + 0.5f);
                if (s_bMaxX - s_bMinX > 0.02f && s_bMaxY - s_bMinY > 0.02f)
                    s_boundsOk = true;
            }
        }
    }

    // ③ 原样恢复
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
    if (bSciss) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (bCull)  glEnable(GL_CULL_FACE);    else glDisable(GL_CULL_FACE);
    if (bDepth) glEnable(GL_DEPTH_TEST);   else glDisable(GL_DEPTH_TEST);
    if (bBlend) glEnable(GL_BLEND);        else glDisable(GL_BLEND);
}

// ---------------- 对外接口 ----------------

bool  Show()  { return s_show; }
void  SetShow(bool v) { s_show = v; }
bool  Ready() { return s_inited && s_tex != 0; }
float Scale() { return s_scale; }
void  SetScale(float v) { s_scale = (v < 0.4f) ? 0.4f : (v > 2.5f ? 2.5f : v); }
float HeadGain() { return s_headGain; }
void  SetHeadGain(float v) { s_headGain = (v < 0.0f) ? 0.0f : (v > 60.0f ? 60.0f : v); }
bool  GazeFollow() { return s_gazeFollow; }
void  SetGazeFollow(bool v) { s_gazeFollow = v; if (!v) { s_tgtX = s_tgtY = 0.0f; } }
bool  AutoBlink() { return s_autoBlink; }
void  SetAutoBlink(bool v) { s_autoBlink = v; }
bool  Talking() { return s_talking; }
void  SetTalking(bool v) { s_talking = v; s_talkPhase = 0.0f; }
void  SetTalkManual(bool v) { s_talkManual = v; }
bool  TalkManual() { return s_talkManual; }
void  SetThinking(bool v) { s_thinking = v; }
bool  Thinking() { return s_thinking; }
const char* StatusText() { return s_status; }

GLuint Tex() { return s_tex; }
bool   BoundsOk() { return s_boundsOk; }

void BoundsNorm(float* x0, float* y0, float* x1, float* y1) {
    if (x0) *x0 = s_bMinX;
    if (y0) *y0 = s_bMinY;
    if (x1) *x1 = s_bMaxX;
    if (y1) *y1 = s_bMaxY;
}

void LastCanvasRect(ImVec2* a, ImVec2* b) {
    if (a) *a = s_canvasA;
    if (b) *b = s_canvasB;
}

void  GazeAt(float nx, float ny) {
    if (!s_gazeFollow) return;
    s_tgtX = (nx < -1.0f) ? -1.0f : (nx > 1.0f ? 1.0f : nx);
    s_tgtY = (ny < -1.0f) ? -1.0f : (ny > 1.0f ? 1.0f : ny);
}

void TriggerIdle() {
    if (s_mgr) if (Live2DModel* m = s_mgr->GetModel()) m->StartRandomMotion("Idle", 1);
}
void TriggerTap() {
    if (s_mgr) if (Live2DModel* m = s_mgr->GetModel()) m->StartRandomMotion("TapBody", 3);
}
void TriggerExpression() {
    if (s_mgr) if (Live2DModel* m = s_mgr->GetModel()) m->SetRandomExpression();
}

void OnContextLost() {
    DropFbo();
    s_needRebuild = true;
}

void OnFrame() {
    if (!s_show) return;
    if (s_needRebuild) {
        s_needRebuild = false;
        RebuildNow();
    }
    if (!EnsureInit()) return;
    if (!EnsureFbo(2048)) return;

    static bool s_wasBusy = false;
    const bool busy   = AiIsBusy();
    const bool stream = AiIsStreaming();
    if (s_wasBusy && !busy) AiTtsFinish(true);   // 一轮结束: 把最后半句也读完
    s_wasBusy = busy;
    SetThinking(busy && !stream);
    SetTalking(stream || Tts::Speaking() || s_talkManual);

    ApplyParameters();
    s_mgr->Update();
    RenderToTexture();
}

// ---------------- 立绘窗口 ----------------
void DrawStandPanel(float px, float py, float pw, float ph) {
    // 只跟随主 UI: 每次都强制到自动位置, 没有独立移动
    ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, CurPalette().SurfaceContainerHigh);
    ImGui::Begin("##live2d_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoSavedSettings);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        const float  pad  = 12.0f;
        const float  barH = 46.0f;

        dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
                          Md3U32(CurPalette().SurfaceContainerHigh), 24.0f);

        // ---- 蓝色标题条 (纯标题, 不可拖动) ----
        {
            const ImU32 barCol = Md3U32(CurPalette().Primary);
            dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + barH),
                              barCol, 24.0f, ImDrawFlags_RoundCornersTop);

            // 三个点 + 标题: 用同一个中心线 cy, 保证水平平齐
            const float cy = wp.y + barH * 0.5f;
            const float dotR = 4.0f;
            for (int i = 0; i < 3; ++i)
                dl->AddCircleFilled(ImVec2(wp.x + 20.0f + i * 13.0f, cy), dotR,
                                    IM_COL32(255, 255, 255, 200));

            // 用字体实际高度算垂直居中, 不再用估算偏移
            const float fs = ImGui::GetFontSize();
            const ImVec2 ts = ImGui::CalcTextSize("立绘");
            dl->AddText(ImVec2(wp.x + 62.0f, cy - ts.y * 0.5f),
                        IM_COL32(255, 255, 255, 245), "立绘");

            if (!Ready()) {
                const ImVec2 ss = ImGui::CalcTextSize(s_status);
                dl->AddText(ImVec2(wp.x + 62.0f + ts.x + 10.0f, cy - ss.y * 0.5f),
                            IM_COL32(255, 255, 255, 175), s_status);
            }
            (void)fs;
        }

        // ---- 画布: 正方形, 占满标题条以下的全部空间 ----
        const float stripW = 16.0f;   // 右侧蓝色拖动长条
        float availW = ws.x - pad * 2.0f - stripW - 8.0f;
        float availH = ws.y - barH - pad * 2.0f;
        float side = (availW < availH) ? availW : availH;
        if (side < 60.0f) side = 60.0f;

        // 画布在剩余空间里水平+垂直都居中
        const float restX = wp.x + pad + stripW + 8.0f;
        const float restY = wp.y + barH + pad;
        const float restW = ws.x - pad * 2.0f - stripW - 8.0f;
        const float restH = ws.y - barH - pad * 2.0f;
        // 水平居中, 但垂直贴顶 (绝不把人物顶到面板下方)
        const ImVec2 imgPos(wp.x + pad + (restW - side) * 0.5f, restY);
        const ImVec2 imgEnd(imgPos.x + side, imgPos.y + side);
        s_canvasA = imgPos;
        s_canvasB = imgEnd;
        dl->AddRectFilled(imgPos, imgEnd, Md3U32(CurPalette().SurfaceContainer), 20.0f);

        if (s_tex) {
            ImGui::SetCursorScreenPos(imgPos);
            ImGui::Image((ImTextureID)(intptr_t)s_tex, ImVec2(side, side), ImVec2(0, 1), ImVec2(1, 0));

            const bool hov = ImGui::IsItemHovered();
            if (hov) {
                const ImVec2 mp = ImGui::GetIO().MousePos;
                GazeAt((mp.x - (imgPos.x + side * 0.5f)) / (side * 0.5f),
                       (mp.y - (imgPos.y + side * 0.5f)) / (side * 0.5f));
            } else if (s_hovering) {
                GazeAt(0.0f, 0.0f);
            }
            s_hovering = hov;
            if (ImGui::IsItemClicked()) TriggerTap();
        } else {
            ImGui::SetCursorScreenPos(ImVec2(imgPos.x + 12.0f, imgPos.y + side * 0.5f - 10.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, CurPalette().OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.8f);
            ImGui::TextUnformatted(s_status);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        {
            const float sx  = wp.x + ws.x - pad - stripW;
            const float sy  = imgPos.y;   // 跟画布同高同起点
            const float sh  = side;                 // 与画布同高, 明显的长条
            const ImVec2 s0(sx, sy), s1(sx + stripW, sy + sh);
            // 未选中段: 半透明主色 (看得出是蓝色的条); 手柄/已选段: 实心主色
            ImVec4 tc = CurPalette().Primary; tc.w = 0.35f;
            const ImU32 trackCol = Md3U32(tc);
            const ImU32 fillCol  = Md3U32(CurPalette().Primary);

            dl->AddRectFilled(s0, s1, trackCol, stripW * 0.5f);

            // 当前位置指示 (0.6~2.0 映射到整条)
            float t = (s_scale - 0.6f) / (2.0f - 0.6f);
            if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
            const float knobH = 64.0f;
            const float knobY = sy + (sh - knobH) * t;
            dl->AddRectFilled(ImVec2(sx, knobY), ImVec2(sx + stripW, knobY + knobH), fillCol, stripW * 0.5f);

            ImGui::SetCursorScreenPos(s0);
            ImGui::PushID(7702);
            ImGui::InvisibleButton("##l2d_strip", ImVec2(stripW, sh));
            const bool sHover = ImGui::IsItemHovered();
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                const float my = ImGui::GetIO().MousePos.y;
                float nt = (my - sy - knobH * 0.5f) / (sh - knobH);
                if (nt < 0.0f) nt = 0.0f; if (nt > 1.0f) nt = 1.0f;
                SetScale(0.6f + nt * (2.0f - 0.6f));
            }
            if (sHover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            ImGui::PopID();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace Live2D
