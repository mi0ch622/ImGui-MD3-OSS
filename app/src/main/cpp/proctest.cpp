// 说明: 使用全新的 ImGui 上下文 + 原版默认皮肤, 与 MD3 主界面完全隔离
#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <GLES2/gl2.h>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#define PTL(...) __android_log_print(ANDROID_LOG_INFO, "PTTEST", __VA_ARGS__)

namespace {
ImGuiContext*  s_ctx   = nullptr;
ANativeWindow* s_win   = nullptr;
JavaVM*        s_vm    = nullptr;
jobject        s_act   = nullptr;
bool           s_ready = false;
float          s_w = 0.0f, s_h = 0.0f;

struct TEv { int a; float x, y; };
std::deque<TEv> s_q;
std::mutex      s_m;

std::string s_result = "点上面的按钮开始检测";
float       s_lastTX = -1.0f, s_lastTY = -1.0f;
bool        s_showDemo = false;
int         s_probes   = 0;

std::vector<char> s_fontBytes;

bool LoadCjkFont(JNIEnv* env, jobject activity, ImGuiIO& io) {
    if (s_fontBytes.empty()) {
        jclass ac = env->GetObjectClass(activity);
        if (ac != nullptr) {
            jmethodID mid = env->GetMethodID(ac, "getAssets", "()Landroid/content/res/AssetManager;");
            jobject amObj = (mid != nullptr) ? env->CallObjectMethod(activity, mid) : nullptr;
            if (amObj != nullptr) {
                AAssetManager* am = AAssetManager_fromJava(env, amObj);
                if (am != nullptr) {
                    AAsset* a = AAssetManager_open(am, "SourceHanSansCN-Bold.otf", AASSET_MODE_BUFFER);
                    if (a != nullptr) {
                        const off_t len = AAsset_getLength(a);
                        if (len > 0) {
                            s_fontBytes.resize((size_t)len);
                            const int got = AAsset_read(a, s_fontBytes.data(), (size_t)len);
                            if (got != (int)len) s_fontBytes.clear();
                        }
                        AAsset_close(a);
                    }
                }
                env->DeleteLocalRef(amObj);
            }
            env->DeleteLocalRef(ac);
        }
        PTL("cjk font bytes=%d", (int)s_fontBytes.size());
    }
    if (s_fontBytes.empty()) return false;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    ImFont* f = io.Fonts->AddFontFromMemoryTTF(s_fontBytes.data(), (int)s_fontBytes.size(),
                                               30.0f, &cfg, io.Fonts->GetGlyphRangesChineseFull());
    return f != nullptr;
}

std::string CallJavaProbe(const char* key) {
    if (s_vm == nullptr || s_act == nullptr) return "JNI 未就绪";
    JNIEnv* env = nullptr;
    bool att = false;
    if (s_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (s_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return "attach 失败";
        att = true;
    }
    std::string out = "ERR";
    jclass cls = env->GetObjectClass(s_act);
    if (cls != nullptr) {
        jmethodID mid = env->GetStaticMethodID(cls, "probeProcess", "(Ljava/lang/String;)Ljava/lang/String;");
        if (mid != nullptr) {
            jstring k = env->NewStringUTF(key ? key : "");
            jstring r = (jstring)env->CallStaticObjectMethod(cls, mid, k);
            if (r != nullptr) {
                const char* cs = env->GetStringUTFChars(r, nullptr);
                if (cs != nullptr) { out = cs; env->ReleaseStringUTFChars(r, cs); }
                env->DeleteLocalRef(r);
            } else {
                out = "返回 null";
            }
            env->DeleteLocalRef(k);
        } else {
            out = "probeProcess 方法未找到";
        }
        env->DeleteLocalRef(cls);
    } else {
        out = "拿不到 Activity class";
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); out = "Java 异常(已清除)"; }
    if (att) s_vm->DetachCurrentThread();
    return out;
}

//   (跨进程模式: 需要 root)
std::string CallJavaPatch(const char* pkg, const char* val) {
    if (s_vm == nullptr || s_act == nullptr) return "JNI 未就绪";
    JNIEnv* env = nullptr;
    bool att = false;
    if (s_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (s_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return "attach 失败";
        att = true;
    }
    std::string out = "ERR";
    jclass cls = env->GetObjectClass(s_act);
    if (cls != nullptr) {
        jmethodID mid = env->GetStaticMethodID(
                cls, "patchTarget", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
        if (mid != nullptr) {
            jstring a = env->NewStringUTF(pkg ? pkg : "");
            jstring b = env->NewStringUTF(val ? val : "");
            jstring r = (jstring)env->CallStaticObjectMethod(cls, mid, a, b);
            if (r != nullptr) {
                const char* cs = env->GetStringUTFChars(r, nullptr);
                if (cs != nullptr) { out = cs; env->ReleaseStringUTFChars(r, cs); }
                env->DeleteLocalRef(r);
            }
            env->DeleteLocalRef(a);
            env->DeleteLocalRef(b);
        } else {
            out = "rootPatch 方法未找到";
        }
        env->DeleteLocalRef(cls);
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); out = "Java 异常(已清除)"; }
    if (att) s_vm->DetachCurrentThread();
    return out;
}

void DrainTouch() {
    std::lock_guard<std::mutex> lk(s_m);
    ImGuiIO& io = ImGui::GetIO();
    for (const TEv& e : s_q) {
        if (e.a == 0) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(e.x, e.y);
            io.AddMouseButtonEvent(0, true);
        } else if (e.a == 1) {
            io.AddMousePosEvent(e.x, e.y);
            io.AddMouseButtonEvent(0, false);
        } else {
            io.AddMousePosEvent(e.x, e.y);
        }
    }
    s_q.clear();
}

// ============================================================================
//   扫描 = 解析 /proc/self/maps 逐段 pread /proc/self/mem 找标签
//   读   = pread /proc/self/mem
//   写   = pwrite /proc/self/mem   ← 这就是“内存修改”的本质
// ============================================================================
struct LabTarget {
    char    tag[24];
    int32_t score;
    int32_t watch;
};

__attribute__((used, visibility("default")))
LabTarget g_lab = { "TARGET_SCORE_01", 1, 0x1234 };

static const char*  kTag    = "TARGET_SCORE_01";
static const size_t kTagOff = 24;                      // score 相对标签的偏移

std::string        s_labLog   = "(还没操作)";
unsigned long long s_tagAddr  = 0;                     // 扫到的标签地址
int                s_readVal  = -99999;                // 从内存读回来的值
bool               s_wroteOk  = false;

// 真实内存扫描
bool ScanSelfTag(unsigned long long& outAddr, std::string& log) {
    int fd = open("/proc/self/mem", O_RDONLY);
    if (fd < 0) { log = "open /proc/self/mem 失败"; return false; }
    FILE* mp = fopen("/proc/self/maps", "r");
    if (mp == nullptr) { close(fd); log = "读 /proc/self/maps 失败"; return false; }

    const size_t TAGLEN = 15;                 // "TARGET_SCORE_01"
    const size_t CH     = 1u << 20;           // 每次读 1MB
    std::vector<char> buf(CH + 64);
    size_t scanned = 0;
    bool   found   = false;
    char   line[512];

    while (fgets(line, sizeof(line), mp) != nullptr) {
        unsigned long a = 0, b = 0; char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s", &a, &b, perms) != 3) continue;
        if (perms[0] != 'r') continue;        // 只看可读段
        if (perms[1] != 'w') continue;
        if (b <= a) continue;
        const size_t len = (size_t)(b - a);
        if (len > (size_t)256 * 1024 * 1024) continue;
        if (scanned > (size_t)512 * 1024 * 1024) break;

        for (size_t off = 0; off < len && !found; off += CH) {
            size_t n = len - off; if (n > CH) n = CH;
            const ssize_t got = pread(fd, buf.data(), n, (off_t)(a + off));
            if (got <= 0) break;
            scanned += (size_t)got;
            for (ssize_t i = 0; i + (ssize_t)TAGLEN <= got; ++i) {
                if (memcmp(buf.data() + i, kTag, TAGLEN) == 0) {
                    outAddr = (unsigned long long)(a + off + (size_t)i);
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }
    fclose(mp); close(fd);

    char tmp[224];
    if (found) snprintf(tmp, sizeof(tmp), "扫描完成(扫了 %zu KB): 标签在 0x%llx", scanned / 1024, outAddr);
    else       snprintf(tmp, sizeof(tmp), "扫描完成(扫了 %zu KB): 没找到标签", scanned / 1024);
    log = tmp;
    return found;
}

// 真实内存读 (4字节 int)
bool MemReadInt(unsigned long long addr, int& out) {
    int fd = open("/proc/self/mem", O_RDONLY);
    if (fd < 0) return false;
    const ssize_t r = pread(fd, &out, sizeof(out), (off_t)addr);
    close(fd);
    return r == (ssize_t)sizeof(out);
}

bool MemWriteInt(unsigned long long addr, int v) {
    int fd = open("/proc/self/mem", O_RDWR);
    if (fd < 0) return false;
    const ssize_t r = pwrite(fd, &v, sizeof(v), (off_t)addr);
    close(fd);
    return r == (ssize_t)sizeof(v);
}

} // namespace
extern "C" {

JNIEXPORT void JNICALL
Java_com_u3dexample_ProcTestActivity_nativePTInit(JNIEnv* env, jobject thiz, jobject surface) {
    if (s_act != nullptr) { env->DeleteGlobalRef(s_act); s_act = nullptr; }
    s_act = env->NewGlobalRef(thiz);
    if (s_vm == nullptr) env->GetJavaVM(&s_vm);

    if (s_ctx == nullptr) { s_ctx = ImGui::CreateContext(); }
    ImGui::SetCurrentContext(s_ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                 // 不写 imgui.ini
    io.DisplaySize = ImVec2(720.0f, 1280.0f);
    if (!LoadCjkFont(env, thiz, io)) {
        ImFontConfig fc; fc.SizePixels = 26.0f;
        io.Fonts->AddFontDefault(&fc);
        PTL("fallback to default font");
    }

    if (s_win != nullptr) { ANativeWindow_release(s_win); s_win = nullptr; }
    if (surface != nullptr) s_win = ANativeWindow_fromSurface(env, surface);
    ImGui_ImplAndroid_Init(s_win);
    ImGui_ImplOpenGL3_Init();
    s_ready = true;
    PTL("pt init done, win=%p", (void*)s_win);
}

JNIEXPORT void JNICALL
Java_com_u3dexample_ProcTestActivity_nativePTResize(JNIEnv*, jobject, jint w, jint h) {
    if (!s_ready) return;
    ImGui::SetCurrentContext(s_ctx);
    s_w = (float)w; s_h = (float)h;
    glViewport(0, 0, w, h);
    ImGui::GetIO().DisplaySize = ImVec2(s_w, s_h);
}

JNIEXPORT void JNICALL
Java_com_u3dexample_ProcTestActivity_nativePTFrame(JNIEnv*, jobject) {
    if (!s_ready) return;
    ImGui::SetCurrentContext(s_ctx);
    ImGuiIO& io = ImGui::GetIO();
    if (s_w > 1.0f && s_h > 1.0f) io.DisplaySize = ImVec2(s_w, s_h);

    DrainTouch();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    // ===================== 原版 ImGui 窗口 (默认皮肤) =====================
    ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 700.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("原版 ImGui - 进程判断测试");

    ImGui::Text("self pid = %d", (int)getpid());
    ImGui::Text("probes   = %d", s_probes);
    ImGui::Text("screen   = %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::Text("touch    = (%.0f, %.0f)", s_lastTX, s_lastTY);
    ImGui::Text("imgui pos= (%.0f, %.0f)", io.MousePos.x, io.MousePos.y);
    ImGui::Separator();

    if (ImGui::Button("判断进程 (列出全部)", ImVec2(280, 60))) {
        s_result = CallJavaProbe("");
        s_probes++;
    }
    ImGui::SameLine();
    if (ImGui::Button("只看游戏", ImVec2(150, 60))) {
        s_result = CallJavaProbe("game");
        s_probes++;
    }
    ImGui::SameLine();
    if (ImGui::Button("清空", ImVec2(90, 60))) {
        s_result = "(已清空)";
    }

    if (ImGui::Button("只看 com.", ImVec2(150, 52)))  { s_result = CallJavaProbe("com."); s_probes++; }
    ImGui::SameLine();
    if (ImGui::Button("只看 unity", ImVec2(150, 52))) { s_result = CallJavaProbe("unity"); s_probes++; }
    ImGui::SameLine();
    if (ImGui::Button("只看 tencent", ImVec2(170, 52))) { s_result = CallJavaProbe("tencent"); s_probes++; }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "内存读写实验室 (本进程靶子 · 无需 root)");
    ImGui::TextWrapped("靶子内存: [TARGET_SCORE_01][int score=1]");
    ImGui::TextWrapped("扫描 = 解析 /proc/self/maps 逐段 pread /proc/self/mem 找标签");
    ImGui::TextWrapped("读   = pread  /proc/self/mem    写 = pwrite /proc/self/mem");

    if (ImGui::Button("① 定位靶子(扫描内存)", ImVec2(250, 58))) {
        unsigned long long a = 0;
        std::string log;
        if (ScanSelfTag(a, log)) { s_tagAddr = a; s_labLog = log; }
        else                     { s_labLog = log; }
    }
    ImGui::SameLine();
    if (ImGui::Button("② 读出 score", ImVec2(160, 58))) {
        if (s_tagAddr == 0) s_labLog = "先点 ① 定位靶子";
        else {
            int v = 0;
            if (MemReadInt(s_tagAddr + kTagOff, v)) {
                s_readVal = v;
                s_labLog = "从内存读回 score = " + std::to_string(v);
            } else s_labLog = "读取失败";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("③ 写入 999", ImVec2(150, 58))) {
        if (s_tagAddr == 0) s_labLog = "先点 ① 定位靶子";
        else {
            if (MemWriteInt(s_tagAddr + kTagOff, 999)) {
                s_wroteOk = true;
                s_labLog = "写入成功 → score 已被改成 999 (看下面的变量值)";
            } else s_labLog = "写入失败";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("重置为 1", ImVec2(110, 58))) {
        g_lab.score = 1;
        s_readVal = 1; s_wroteOk = false;
        s_labLog = "靶子已重置为 1";
    }

    ImGui::Text("标签地址   = 0x%llx", s_tagAddr);
    ImGui::Text("score 地址 = 0x%llx", s_tagAddr ? (s_tagAddr + kTagOff) : 0ULL);
    ImGui::Text("内存读回值 = %d", s_readVal);
    ImGui::TextColored(s_wroteOk ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
                       "靶子变量现值 = %d", g_lab.score);
    ImGui::TextWrapped("日志: %s", s_labLog.c_str());

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                       "跨进程模式 —— 直接读写 /proc/PID/mem (不做 root 前置判断)");
    static char s_target[128] = "com.u3dexample.practice";
    static char s_val[24] = "999";
    ImGui::Text("目标 (包名, 或直接填 PID 数字):");
    ImGui::SetNextItemWidth(520);
    ImGui::InputText("##tg", s_target, sizeof(s_target));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##vv", s_val, sizeof(s_val));
    ImGui::SameLine();
    ImGui::TextDisabled("写入值");

    if (ImGui::Button("识别PID + 扫可写段 + 写入", ImVec2(400, 58))) {
        s_result = CallJavaPatch(s_target, s_val);
        s_probes++;
    }
    ImGui::SameLine();
    if (ImGui::Button("只识别 PID", ImVec2(180, 58))) {
        s_result = "本进程 self_pid = " + std::to_string((long)getpid());
    }
    ImGui::TextWrapped("结果看最下方“结果”框 (权限不够时会报真实的 errno ✗)");

    ImGui::Checkbox("顺便打开原版 Demo 窗口", &s_showDemo);

    ImGui::Separator();
    ImGui::Text("结果:");
    ImGui::BeginChild("##res", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(s_result.c_str());
    ImGui::EndChild();
    ImGui::End();

    if (s_showDemo) ImGui::ShowDemoWindow(&s_showDemo);

    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

JNIEXPORT void JNICALL
Java_com_u3dexample_ProcTestActivity_nativePTTouch(JNIEnv*, jobject, jint a, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lk(s_m);
    s_lastTX = x; s_lastTY = y;
    s_q.push_back(TEv{(int)a, x, y});
}

} // extern "C"
