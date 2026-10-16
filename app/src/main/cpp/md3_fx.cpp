// =====================================================================
// md3_fx.cpp
//   ① 真实流星雨特效 (全屏前景层, 分段渐变拖尾)
//   ② 卡密本地验证门禁 (输入框+解密卡密+购买卡密, 错误→红色抖动)
// =====================================================================
#include "md3_common.h"
#include <cmath>
#include <cstring>
#include <cctype>
#include <jni.h>
#include <cstdlib>
#include <string>
#include "imgui_internal.h"

// =====================================================================
//  ① 流星雨
// =====================================================================
namespace Fx {

struct Meteor {
    float x, y, vx, vy;
    float len, life, maxLife, w, hue;
    bool  alive;
};
static const int MAXM = 26;
static Meteor s_m[MAXM];
static bool   s_on = false;
static float  s_spawn = 0.0f;
static unsigned s_seed = 12345u;

static float Rnd() {                       // xorshift, 不依赖 rand()
    s_seed ^= s_seed << 13; s_seed ^= s_seed >> 17; s_seed ^= s_seed << 5;
    return (float)(s_seed % 100000u) / 100000.0f;
}

bool On() { return s_on; }
void SetOn(bool v) { s_on = v; if (!v) for (int i = 0; i < MAXM; ++i) s_m[i].alive = false; }

static void SpawnOne(float W, float H) {
    for (int i = 0; i < MAXM; ++i) {
        if (s_m[i].alive) continue;
        Meteor& m = s_m[i];
        const float sp = 520.0f + Rnd() * 620.0f;          // 速度 px/s
        const float ang = 0.52f + Rnd() * 0.22f;           // 约 30~42 度斜落
        m.vx = cosf(ang) * sp;
        m.vy = sinf(ang) * sp;
        // 从上方或左侧之外出发
        if (Rnd() < 0.62f) { m.x = -80.0f + Rnd() * W * 1.15f; m.y = -60.0f - Rnd() * 120.0f; }
        else               { m.x = -120.0f - Rnd() * 260.0f;   m.y = -40.0f + Rnd() * H * 0.45f; }
        m.len = 150.0f + Rnd() * 240.0f;
        m.maxLife = 1.7f + Rnd() * 1.3f;
        m.life = m.maxLife;
        m.w = 1.6f + Rnd() * 1.9f;
        m.hue = Rnd();
        m.alive = true;
        return;
    }
}

void Update(float dt, float W, float H) {
    if (!s_on) return;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.05f) dt = 0.05f;

    s_spawn += dt;
    while (s_spawn > 0.11f) { s_spawn -= 0.11f; SpawnOne(W, H); }

    for (int i = 0; i < MAXM; ++i) {
        Meteor& m = s_m[i];
        if (!m.alive) continue;
        m.x += m.vx * dt;
        m.y += m.vy * dt;
        m.life -= dt;
        if (m.life <= 0.0f || m.y > H + m.len || m.x > W + m.len) m.alive = false;
    }
}

void Draw(ImDrawList* fg, float W, float H) {
    if (!s_on) return;
    for (int i = 0; i < MAXM; ++i) {
        Meteor& m = s_m[i];
        if (!m.alive) continue;

        const float sp = sqrtf(m.vx * m.vx + m.vy * m.vy);
        if (sp < 1.0f) continue;
        const float ux = m.vx / sp, uy = m.vy / sp;      // 单位方向

        // 头部淡入淡出: 刚出生 / 快消失时都淡
        float k = 1.0f;
        const float born = m.maxLife - m.life;
        if (born < 0.25f) k = born / 0.25f;
        const float fade = m.life / 0.45f;
        if (fade < 1.0f) k *= fade;

        // 颜色: 白青/白金 冷暖微变, 看起来高级
        const int cr = 210 + (int)(m.hue * 45);
        const int cg = 236 + (int)((1.0f - m.hue) * 18);
        const int cb = 255;

        const int SEGS = 18;
        const float segLen = m.len / (float)SEGS;
        for (int s = 0; s < SEGS; ++s) {
            const float t0 = (float)s / SEGS;
            const float t1 = (float)(s + 1) / SEGS;
            const ImVec2 a(m.x - ux * (m.len * t0), m.y - uy * (m.len * t0));
            const ImVec2 b(m.x - ux * (m.len * t1), m.y - uy * (m.len * t1));
            const float alpha = (1.0f - t0) * (1.0f - t0) * 0.95f * k;   // 平方衰减
            const float wdt = m.w * (1.0f - t0 * 0.75f);
            if (alpha <= 0.01f) continue;
            fg->AddLine(a, b, IM_COL32(cr, cg, cb, (int)(alpha * 255)), wdt);
        }

        // 亮头 + 柔光
        fg->AddCircleFilled(ImVec2(m.x, m.y), m.w * 1.7f, IM_COL32(255, 255, 255, (int)(230 * k)));
        fg->AddCircleFilled(ImVec2(m.x, m.y), m.w * 5.0f, IM_COL32(cr, cg, cb, (int)(52 * k)));
        fg->AddCircleFilled(ImVec2(m.x, m.y), m.w * 10.0f, IM_COL32(cr, cg, cb, (int)(20 * k)));
    }
}

} // namespace Fx

// =====================================================================
//  ② 卡密门禁
// =====================================================================
namespace Lic {

static bool  s_unlocked = false;
static char  s_input[64] = "";
static float s_shake = 0.0f;       // 屏幕震动强度 (衰减)
static float s_err = 0.0f;         // 红色闪烁 (衰减)
static float s_inAnim = 0.0f;      // 页面入场动画
static int   s_open = -1;          // 展开的区块: 0输入 1解密 2购买
static bool  s_forced = false;     // 是否因卡密被拦下

bool  Unlocked() { return s_unlocked; }
float ShakeAmp() { return s_shake; }
float ErrAmount() { return s_err; }
float IntroT()   { return s_inAnim; }

void SetLocked(bool v) { s_unlocked = !v; if (v) { s_shake = 0.0f; s_err = 0.0f; } }

void NotifyWrong() {
    s_shake = 1.0f;                 // 屏幕猛地一抖
    s_err = 1.0f;                   // 红光一闪
}

void OnEnvelope(const char* s) {
}

void Update(float dt) {
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (s_shake > 0.0f) s_shake -= dt * 2.6f;
    if (s_shake < 0.0f) s_shake = 0.0f;
    if (s_err > 0.0f) s_err -= dt * 1.5f;
    if (s_err < 0.0f) s_err = 0.0f;
    if (!s_unlocked) s_inAnim += dt * 2.2f;
    else s_inAnim = 0.0f;
}

void OpenUrl(const char* url);

// ---- 卡密页 ----
void DrawPage() {
    const Md3Palette& p = CurPalette();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float t = (s_inAnim < 1.0f) ? s_inAnim : 1.0f;
    // 入场: 从下往上浮 + 淡入
    const float slide = (1.0f - t) * 42.0f;

    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    // 顶部大标题
    ImGui::SetCursorScreenPos(ImVec2(o.x + 8.0f, o.y + 18.0f - slide));
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.55f,
                ImGui::GetCursorScreenPos(), Md3U32(p.OnSurface), TL("卡密验证", "License"));
    ImGui::SetCursorScreenPos(ImVec2(o.x + 10.0f, o.y + 62.0f - slide));
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.82f,
                ImGui::GetCursorScreenPos(), Md3U32(p.OnSurfaceVariant),
                TL("输入任意卡密即可解锁全部页面（本地验证）",
                   "Enter any license key to unlock everything (local check)"));

    const float cardX = o.x + 10.0f;
    const float cardW = avail.x - 20.0f;
    float y = o.y + 104.0f - slide;

    // ① 输入栏
    {
        const float h = 150.0f;
        dl->AddRectFilled(ImVec2(cardX, y), ImVec2(cardX + cardW, y + h),
                          Md3U32(p.SurfaceContainerHigh), 20.0f);
        if (s_err > 0.0f)
            dl->AddRect(ImVec2(cardX, y), ImVec2(cardX + cardW, y + h),
                        IM_COL32(255, 60, 60, (int)(s_err * 230)), 20.0f, 0, 3.0f);

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cardX + 18.0f, y + 16.0f), Md3U32(p.OnSurface),
                    TL("卡密", "License key"));

        ImGui::SetCursorScreenPos(ImVec2(cardX + 18.0f, y + 46.0f));
        ImGui::SetNextItemWidth(cardW - 150.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Md3U32(p.SurfaceContainerHigh));
        ImGui::PushStyleColor(ImGuiCol_Text, Md3U32(p.OnSurface));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        ImGui::InputText("##lic_input", s_input, sizeof(s_input));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SetCursorScreenPos(ImVec2(cardX + cardW - 124.0f, y + 46.0f));
        if (Md3Button(TL("解锁", "Unlock"), ImVec2(106.0f, 38.0f))) {
            std::string v(s_input);
            bool ok = false;
            for (char ch : v) if (!isspace((unsigned char)ch)) { ok = true; break; }
            if (ok) { s_unlocked = true; s_shake = 0.0f; s_err = 0.0f; PushLog(LOG_INFO, "[卡密] 验证通过 (本地)"); }
            else    { NotifyWrong(); PushLog(LOG_WARN, "[卡密] 空卡密，已拒绝"); }
        }

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.74f,
                    ImVec2(cardX + 18.0f, y + 104.0f),
                    s_err > 0.0f ? IM_COL32(255, 90, 90, 255) : Md3U32(p.OnSurfaceVariant),
                    s_err > 0.0f ? TL("卡密不能为空", "License key cannot be empty")
                                 : TL("本地验证：任意非空卡密均可通过", "Local check: any non-empty key passes"));
        y += h + 14.0f;
    }

    // ② 解密卡密
    {
        const float h = (s_open == 1) ? 168.0f : 62.0f;
        dl->AddRectFilled(ImVec2(cardX, y), ImVec2(cardX + cardW, y + h),
                          Md3U32(p.SurfaceContainerHigh), 20.0f);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cardX + 18.0f, y + 20.0f), Md3U32(p.OnSurface),
                    TL("解密卡密", "Decode key"));
        ImGui::SetCursorScreenPos(ImVec2(cardX, y));
        ImGui::PushID(8801);
        if (ImGui::InvisibleButton("##lic_dec", ImVec2(cardW, 62.0f)))
            s_open = (s_open == 1) ? -1 : 1;
        ImGui::PopID();
        if (s_open == 1) {
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.82f,
                        ImVec2(cardX + 18.0f, y + 54.0f), Md3U32(p.OnSurfaceVariant),
                        TL("把卡密粘贴到上方输入框，点「解锁」即可。",
                           "Paste your key above and press Unlock."));
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.82f,
                        ImVec2(cardX + 18.0f, y + 82.0f), Md3U32(p.OnSurfaceVariant),
                        TL("若卡密已过期，请联系作者重新获取。",
                           "If it expired, contact the author for a new one."));
        }
        y += h + 14.0f;
    }

    // ③ 购买卡密 (跳转网页)
    {
        const float h = 62.0f;
        dl->AddRectFilled(ImVec2(cardX, y), ImVec2(cardX + cardW, y + h),
                          Md3U32(p.PrimaryContainer), 20.0f);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cardX + 18.0f, y + 20.0f), Md3U32(p.OnPrimaryContainer),
                    TL("购买卡密", "Buy a key"));
        ImGui::SetCursorScreenPos(ImVec2(cardX, y));
        ImGui::PushID(8802);
        if (ImGui::InvisibleButton("##lic_buy", ImVec2(cardW, h))) {
            OpenUrl("https://wiki.ottohub.cn/%E6%A3%8D%E6%AF%8D");
            PushLog(LOG_INFO, "[卡密] 打开购买页");
        }
        ImGui::PopID();
    }
}

} // namespace Lic

// =====================================================================
// =====================================================================
namespace UiCmd {

static void Log(const char* s) { PushLog(LOG_INFO, "[AI控制] %s", s); }

bool Exec(const std::string& key, const std::string& val) {
    if (key == "nav") {
        if      (val == "home")  SetNavIndex(NAV_HOME);
        else if (val == "battle")SetNavIndex(NAV_TARGET);
        else if (val == "draw")  SetNavIndex(NAV_DRAW);
        else if (val == "world") SetNavIndex(NAV_GLOBE);
        else if (val == "terminal") SetNavIndex(NAV_TERMINAL);
        else if (val == "settings") SetNavIndex(NAV_SETTINGS);
        else return false;
        Log("切页"); return true;
    }
    if (key == "esp") { EspSetMaster(val == "on" || val == "true" || val == "1"); Log("绘制总开关"); return true; }
    if (key == "blur") { MotionBlur::SetEnabled(val == "on" || val == "true" || val == "1"); Log("运动模糊"); return true; }
    if (key == "live2d") { Live2D::SetShow(val == "on" || val == "true" || val == "1"); Log("立绘"); return true; }
    if (key == "tts") { Tts::SetEnabled(val == "on" || val == "true" || val == "1"); Log("语音"); return true; }
    if (key == "meteor") { Fx::SetOn(val == "on" || val == "true" || val == "1"); Log("流星雨"); return true; }
    if (key == "dark") { SetDarkMode(val == "on" || val == "true" || val == "1"); Log("深色模式"); return true; }
    if (key == "restore" || key == "reset") { RestoreAll(); Log("一键恢复"); return true; }
    return false;
}

void RestoreAll() {
    SetNavIndex(NAV_HOME);
    EspSetMaster(false);
    MotionBlur::SetEnabled(false);
    Live2D::SetShow(true);
    Live2D::SetScale(1.35f);
    Live2D::SetHeadGain(30.0f);
    Live2D::SetGazeFollow(true);
    Live2D::SetAutoBlink(true);
    Tts::SetEnabled(true);
    Tts::SetLoli(true);
    Tts::SetPitch(1.55f);
    Tts::SetRate(1.08f);
    Fx::SetOn(false);
    SetDarkMode(true);
    Lic::SetLocked(false);
    PushLog(LOG_INFO, "[AI控制] 已一键恢复默认设置");
}

// 扫文本里的 [[ui:key=val]] 并执行
int ScanAndExec(const std::string& text) {
    int n = 0;
    size_t pos = 0;
    while (true) {
        size_t a = text.find("[[ui:", pos);
        if (a == std::string::npos) break;
        size_t b = text.find("]]", a);
        if (b == std::string::npos) break;
        std::string body = text.substr(a + 5, b - a - 5);
        size_t eq = body.find('=');
        const std::string key = (eq == std::string::npos) ? body : body.substr(0, eq);
        const std::string val = (eq == std::string::npos) ? std::string() : body.substr(eq + 1);
        if (Exec(key, val)) ++n;
        pos = b + 2;
    }
    return n;
}

} // namespace UiCmd

// ---- 卡密页: 打开购买网页 (走 Java 的 Intent) ----
namespace Lic {
void OpenUrl(const char* url) {
    if (g_jvm == nullptr || g_activityRef == nullptr || url == nullptr) return;
    bool attached = false;
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    jclass cls = env->GetObjectClass(g_activityRef);
    if (cls) {
        jmethodID mid = env->GetMethodID(cls, "openUrl", "(Ljava/lang/String;)V");
        if (mid) {
            jstring js = env->NewStringUTF(url);
            env->CallVoidMethod(g_activityRef, mid, js);
            if (js) env->DeleteLocalRef(js);
        }
        env->DeleteLocalRef(cls);
    }
    if (attached) g_jvm->DetachCurrentThread();
}

// ---- 整屏抖动: 直接偏移一帧的所有顶点 (与折叠动画同一条路子) ----
void ApplyShake(ImDrawData* dd) {
    if (dd == nullptr) return;
    const float a = s_shake;
    if (a <= 0.001f) return;
    const float t = (float)ImGui::GetTime();
    // 高频衰减震动, 越抖越小
    const float amp = a * a * 26.0f;
    const float dx = sinf(t * 63.0f) * amp + sinf(t * 31.0f) * amp * 0.5f;
    const float dy = cosf(t * 71.0f) * amp * 0.65f + cosf(t * 27.0f) * amp * 0.35f;
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        ImDrawList* cl = dd->CmdLists[n];
        ImDrawVert* v = cl->VtxBuffer.Data;
        for (int i = 0; i < cl->VtxBuffer.Size; ++i) {
            v[i].pos.x += dx;
            v[i].pos.y += dy;
        }
    }
}

} // namespace Lic
