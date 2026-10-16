// =====================================================================
// md3_motionblur.cpp - 全屏帧反馈运动模糊（Motion Blur / 拖影）
// ---------------------------------------------------------------------
// 目标：静止内容保持绝对锐利，只有"运动中的像素"拖出残影。
//
// 原理（帧反馈）：
//     out = mix(prev, cur, k)          k = 1 - strength * 0.85
//   - prev == cur（没动过的像素）时：mix(p,p,k) == p，**数学上完全相等**
//     → 静止区域一帧帧收敛成原图，零糊、零漂移
//   - prev != cur（动过的像素）时：落后的残影被反复混合 → 拖尾
//
// 关键实现点（这版修正了"整屏轻微发糊"的 bug）：
//   1) 拖影缓冲必须是 **全分辨率**。之前用半分辨率 + LINEAR 放大回全屏，
//      等于每帧都在和一张模糊图混合 → 静止画面永远糊着一层。
//   2) 反馈用 glCopyTexSubImage2D 从"刚画到屏幕的结果"1:1 拷贝回 prev，
//      不做缩放、不做采样 → 静止像素逐位相等 → 零糊。
//   3) 不做 ping-pong 渲染反馈，避免读到自身正在写入的纹理（未定义行为）。
//
// 流程：
//   BeginCapture()           ImGui 渲染到 FBO(texCur)
//   EndCaptureAndPresent()   ① out = mix(texPrev, texCur, k) → 屏幕
//                            ② 屏幕结果 → glCopyTexSubImage2D → texPrev
// =====================================================================
#include "md3_common.h"
#include <GLES3/gl3.h>
#include <cstdio>

namespace MotionBlur {

// ---------------- 状态 ----------------
static bool   s_enabled   = false;
static float  s_strength  = 0.55f;
static int    s_w = 0, s_h = 0;

static GLuint s_fboCur  = 0, s_texCur = 0;   // 当前帧：ImGui 渲染进这个 FBO
static GLuint s_texPrev = 0;                 // 上一帧"显示结果"的全分辨率拷贝（反馈源）

static bool   s_needClear = true;            // 刚启用 / 刚建缓冲：首帧纯拷贝，别混脏数据
static bool   s_broken    = false;           // FBO 不完整等致命问题 → 永久降级
static GLenum s_fboStatus = 0;
static int    s_fboW = 0, s_fboH = 0;
static int    s_wantW = 0, s_wantH = 0;      // 期望尺寸（跨上下文丢失保留）
static bool   s_capturing = false;           // 本帧是否真的切进了 FBO

// ---------------- GL 资源 ----------------
static GLuint s_prog = 0, s_vbo = 0;
static GLint  s_aPos = -1, s_uPrev = -1, s_uCur = -1, s_uMix = -1;

// 顶点着色器：全屏四边形，直接把 NDC 当 UV 用（1:1，无缩放无模糊）
static const char* kVS =
    "attribute vec2 aPos;\n"
    "varying vec2 vUv;\n"
    "void main(){\n"
    "  vUv = aPos * 0.5 + 0.5;\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

// 片元着色器：两帧混合
static const char* kFS =
    "precision mediump float;\n"
    "varying vec2 vUv;\n"
    "uniform sampler2D uPrev;\n"
    "uniform sampler2D uCur;\n"
    "uniform float uMix;\n"
    "void main(){\n"
    "  vec4 c = texture2D(uCur,  vUv);\n"
    "  vec4 p = texture2D(uPrev, vUv);\n"
    "  gl_FragColor = mix(p, c, uMix);\n"
    "}\n";

static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(s); return 0; }
    return s;
}

static void Release() {
    if (s_fboCur)  { glDeleteFramebuffers(1, &s_fboCur); s_fboCur = 0; }
    if (s_texCur)  { glDeleteTextures(1, &s_texCur); s_texCur = 0; }
    if (s_texPrev) { glDeleteTextures(1, &s_texPrev); s_texPrev = 0; }
    s_w = s_h = 0;
}

static bool EnsureProgram() {
    if (s_prog) return true;
    GLuint vs = Compile(GL_VERTEX_SHADER, kVS);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    s_prog = glCreateProgram();
    glAttachShader(s_prog, vs);
    glAttachShader(s_prog, fs);
    glBindAttribLocation(s_prog, 0, "aPos");
    glLinkProgram(s_prog);
    GLint ok = 0;
    glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) { glDeleteProgram(s_prog); s_prog = 0; return false; }

    s_aPos  = glGetAttribLocation(s_prog, "aPos");
    s_uPrev = glGetUniformLocation(s_prog, "uPrev");
    s_uCur  = glGetUniformLocation(s_prog, "uCur");
    s_uMix  = glGetUniformLocation(s_prog, "uMix");

    // 全屏四边形（TRIANGLE_STRIP，4 顶点）
    const float quad[8] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

static GLuint MakeTex(int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static void DrawFullscreenQuad() {
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(s_aPos);
    glVertexAttribPointer(s_aPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------- 对外接口 ----------------

static void DropHandles() {
    // 上下文可能已销毁：只把句柄清零，绝不调 glDelete
    // （对死上下文调 glDelete 是无害的，但对新上下文可能误删同号对象）
    s_prog = 0; s_vbo = 0;
    s_aPos = -1; s_uPrev = s_uCur = s_uMix = -1;
    s_fboCur = 0; s_texCur = 0; s_texPrev = 0;
    s_fboStatus = 0;
    s_broken = false;      // 换上下文后重新自检
    s_needClear = true;
}

static bool HandlesValid() {
    return s_prog && s_texCur && s_texPrev && s_fboCur &&
           glIsProgram(s_prog) && glIsTexture(s_texCur) &&
           glIsTexture(s_texPrev) && glIsFramebuffer(s_fboCur);
}

static void CreateAll(int w, int h) {
    s_w = w;
    s_h = h;

    // (1) 当前帧：全分辨率纹理 + FBO
    s_texCur = MakeTex(w, h);
    glGenFramebuffers(1, &s_fboCur);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fboCur);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_texCur, 0);
    s_fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    s_fboW = w;
    s_fboH = h;
    s_broken = (s_fboStatus != GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // (2) 反馈源：全分辨率纹理（只被 glCopyTexSubImage2D 写入，只被采样读取）
    s_texPrev = MakeTex(w, h);

    glViewport(0, 0, w, h);
    s_needClear = true;
}

/** 资源保证：切后台/息屏/旋转导致上下文丢失后，这里会自动重建 */
static bool EnsureResources() {
    if (s_wantW <= 0 || s_wantH <= 0) return false;
    if (HandlesValid()) return true;

    DropHandles();
    if (!EnsureProgram()) return false;
    CreateAll(s_wantW, s_wantH);
    return HandlesValid();
}

void SetScreenSize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    s_wantW = w;
    s_wantH = h;
    if (HandlesValid() && w == s_w && h == s_h) return;

    if (HandlesValid()) Release();   // 尺寸变了：正常释放再建
    else DropHandles();
    if (!EnsureProgram()) return;
    CreateAll(w, h);
}

void SetEnabled(bool e) {
    if (s_enabled == e) return;
    s_enabled = e;
    s_needClear = true;   // 每次开启都重新"起步"，不拖上一次的残影
}

void SetStrength(float v) {
    s_strength = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float Strength()   { return s_strength; }
bool  Enabled()    { return s_enabled && !s_broken; }
bool  IsBroken()   { return s_broken; }
bool  Capturing()  { return s_capturing; }

/** 给设置页看的一行状态（调试用） */
const char* StatusText() {
    static char buf[160];
    snprintf(buf, sizeof(buf), "%s %dx%d FBO=0x%04X",
             s_broken ? "已降级" : (s_enabled ? "开" : "关"),
             s_fboW, s_fboH, (unsigned)s_fboStatus);
    return buf;
}

/** 新 GL 上下文建立（切后台回来 / 息屏唤醒 / nativeInit）时调用 */
void OnContextLost() {
    DropHandles();
    s_w = s_h = 0;        // 期望尺寸 s_wantW/s_wantH 保留 → 下一帧自动重建
    s_capturing = false;
}

void BeginCapture() {
    s_capturing = false;
    if (!Enabled()) return;
    if (!EnsureResources()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, s_fboCur);
    glViewport(0, 0, s_w, s_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   // 透明底，保留与桌面的合成
    glClear(GL_COLOR_BUFFER_BIT);
    s_capturing = true;
}

void EndCaptureAndPresent() {
    if (!s_capturing) return;      // 没真进 FBO 就绝不覆盖屏幕
    s_capturing = false;
    if (!Enabled() || !s_prog) return;
    if (!HandlesValid()) return;   // 资源异常：本帧放弃合成，屏幕保留上一帧

    const float k = 1.0f - s_strength * 0.85f;   // 0 → 不混；1 → k=0.15 长拖尾

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, s_w, s_h);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(s_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texPrev);
    glUniform1i(s_uPrev, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_texCur);
    glUniform1i(s_uCur, 1);

    // 首帧 uMix = 1：直接输出当前帧（不做混合），避免读到上一轮的脏残影
    glUniform1f(s_uMix, s_needClear ? 1.0f : k);

    DrawFullscreenQuad();

    // 反馈：把"刚刚显示出来的结果"整份 1:1 拷回 prev
    // （不缩放、不采样 → 静止像素逐位相等 → 下一帧 mix(p,p,k)==p，零糊）
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texPrev);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, s_w, s_h);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(0);
    glEnable(GL_BLEND);

    s_needClear = false;
}

} // namespace MotionBlur
