// =====================================================================
// md3_tts.cpp - 立绘语音播报 (调用 Java 层的 android TextToSpeech)
//   * 萝莉音: 音调偏高(1.55) + 语速稍快(1.08)
//   * 流式逐句播放: md3_ai.cpp 每凑满一句就丢进来
//   * 预估“正在说话”的时长 → 驱动立绘嘴巴开合
// =====================================================================
#include "md3_common.h"
#include <jni.h>
#include <string>
#include <chrono>

namespace Tts {

static bool  s_enabled = true;
static bool  s_loli    = true;
static float s_pitch    = 1.55f;
static float s_rate     = 1.08f;
static bool  s_javaOk   = false;
static long long s_speakUntilMs = 0;

static long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static JNIEnv* Env(bool* attached) {
    *attached = false;
    if (g_jvm == nullptr) return nullptr;
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
        *attached = true;
    }
    return env;
}

static void CallVoid(const char* name, const char* sig, const char* strArg, float fArg) {
    if (g_activityRef == nullptr) return;
    bool attached = false;
    JNIEnv* env = Env(&attached);
    if (!env) return;
    jclass cls = env->GetObjectClass(g_activityRef);
    if (cls) {
        jmethodID mid = env->GetMethodID(cls, name, sig);
        if (mid) {
            if (strArg) {
                jstring js = env->NewStringUTF(strArg);
                env->CallVoidMethod(g_activityRef, mid, js);
                if (js) env->DeleteLocalRef(js);
            } else {
                env->CallVoidMethod(g_activityRef, mid, fArg);
            }
        }
        env->DeleteLocalRef(cls);
    }
    if (attached) g_jvm->DetachCurrentThread();
}

void Init() {
    if (s_javaOk) return;
    CallVoid("ttsInit", "()V", nullptr, 0.0f);
    CallVoid("ttsSetPitch", "(F)V", nullptr, s_pitch);
    CallVoid("ttsSetRate", "(F)V", nullptr, s_rate);
    s_javaOk = true;
}

void Say(const std::string& text) {
    if (!s_enabled) return;
    if (text.empty()) return;
    Init();
    CallVoid("ttsSay", "(Ljava/lang/String;)V", text.c_str(), 0.0f);
    // 预估朗读时长(约 180ms/字, 上下限)
    long long dur = (long long)text.size() * 180LL;
    if (dur < 600LL) dur = 600LL;
    if (dur > 20000LL) dur = 20000LL;
    s_speakUntilMs = NowMs() + dur;
}

void Stop() {
    CallVoid("ttsStop", "()V", nullptr, 0.0f);
    s_speakUntilMs = 0;
}

// 预估“现在正在朗读” → 驱动立绘嘴巴
bool Speaking() { return s_enabled && NowMs() < s_speakUntilMs; }

bool  Enabled() { return s_enabled; }
void  SetEnabled(bool v) { s_enabled = v; if (!v) { Stop(); } }

bool  Loli() { return s_loli; }
void  SetLoli(bool v) {
    s_loli = v;
    s_pitch = v ? 1.55f : 1.00f;
    s_rate  = v ? 1.08f : 1.00f;
    if (s_javaOk) {
        CallVoid("ttsSetPitch", "(F)V", nullptr, s_pitch);
        CallVoid("ttsSetRate", "(F)V", nullptr, s_rate);
    }
}

float Pitch() { return s_pitch; }
void  SetPitch(float v) {
    s_pitch = (v < 0.5f) ? 0.5f : (v > 2.0f ? 2.0f : v);
    if (s_javaOk) CallVoid("ttsSetPitch", "(F)V", nullptr, s_pitch);
}

float Rate() { return s_rate; }
void  SetRate(float v) {
    s_rate = (v < 0.5f) ? 0.5f : (v > 2.0f ? 2.0f : v);
    if (s_javaOk) CallVoid("ttsSetRate", "(F)V", nullptr, s_rate);
}

} // namespace Tts
