// md3_mcp.cpp - 设置页「工作区 & MCP 服务器」
//   * 工作区：显示当前路径与可读写状态，支持系统目录选择器 / 手输路径 / 还原默认
//   * MCP：服务器列表（名称·地址·开关·测试·删除）+ 内置真实开源预设
//   * 配置全部存在 Java 侧，这里用索引式 JNI 读写，避免在 C++ 里解析 JSON
#include "md3_common.h"
#include <cstdio>
#include <cstring>

namespace {

struct McpItem {
    std::string name;
    std::string url;
    bool        enabled;
};

std::vector<McpItem> g_items;
bool                 g_loaded    = false;
bool                 g_wsLoaded  = false;
std::string          g_wsPath, g_wsStatus, g_wsDefault;
int                  g_testIdx   = -1;
std::string          g_testMsg;
char                 g_wsEdit[256] = "";
bool                 g_wsEditInit  = false;
int                  g_presetPick  = 0;
bool                 g_editOpen    = false;   // 编辑弹窗
int                  g_editIdx     = -1;
bool                 g_openEditForLast = false;
char                 g_editName[128] = "";
char                 g_editUrl[256]  = "";

JNIEnv* Attach(bool& attached) {
    attached = false;
    if (g_jvm == nullptr) return nullptr;
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
        attached = true;
    }
    return env;
}

void Detach(bool attached) { if (attached) g_jvm->DetachCurrentThread(); }

jclass Cls(JNIEnv* env, const char* name) {
    jclass c = env->FindClass(name);
    if (c == nullptr && env->ExceptionCheck()) env->ExceptionClear();
    return c;
}

std::string JStr(JNIEnv* env, jstring js) {
    std::string out;
    if (js == nullptr) return out;
    const char* p = env->GetStringUTFChars(js, nullptr);
    if (p != nullptr) { out = p; env->ReleaseStringUTFChars(js, p); }
    env->DeleteLocalRef(js);
    return out;
}

// ---------- 读取：MCP 列表 ----------
int McpCount() {
    bool att = false;
    JNIEnv* env = Attach(att);
    int n = 0;
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "count", "()I");
            if (m) n = env->CallStaticIntMethod(c, m);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    return n;
}

std::string McpField(int i, const char* key) {
    bool att = false;
    JNIEnv* env = Attach(att);
    std::string out;
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "field",
                    "(ILjava/lang/String;)Ljava/lang/String;");
            if (m) {
                jstring k = env->NewStringUTF(key);
                jstring r = (jstring)env->CallStaticObjectMethod(c, m, (jint)i, k);
                out = JStr(env, r);
                env->DeleteLocalRef(k);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    return out;
}

bool McpEnabled(int i) {
    bool att = false;
    JNIEnv* env = Attach(att);
    bool r = false;
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "isEnabled", "(I)Z");
            if (m) r = (env->CallStaticBooleanMethod(c, m, (jint)i) == JNI_TRUE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    return r;
}

// ---------- 写入动作 ----------
void McpSetEnabled(int i, bool v) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "setEnabled", "(IZ)V");
            if (m) env->CallStaticVoidMethod(c, m, (jint)i, v ? JNI_TRUE : JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
}

void McpCallStr(const char* method, const char* sig, const char* a) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, method, sig);
            if (m) {
                jstring js = env->NewStringUTF(a == nullptr ? "" : a);
                env->CallStaticVoidMethod(c, m, js);
                env->DeleteLocalRef(js);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
}

void McpCallInt(const char* method, const char* sig, int i, bool b) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, method, sig);
            if (m) env->CallStaticVoidMethod(c, m, (jint)i, b ? JNI_TRUE : JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
}

void ReloadMcp() {
    g_items.clear();
    int n = McpCount();
    for (int i = 0; i < n && i < 40; i++) {
        McpItem it;
        it.name    = McpField(i, "name");
        it.url     = McpField(i, "url");
        it.enabled = McpEnabled(i);
        g_items.push_back(it);
    }
    g_loaded = true;
    //「自定义」添加完之后自动弹出编辑框
    if (g_openEditForLast) {
        g_openEditForLast = false;
        if (!g_items.empty()) {
            g_editIdx = (int)g_items.size() - 1;
            snprintf(g_editName, sizeof(g_editName), "%s", g_items[g_editIdx].name.c_str());
            snprintf(g_editUrl, sizeof(g_editUrl), "%s", g_items[g_editIdx].url.c_str());
            g_editOpen = true;
        }
    }
}

void ReloadWs() {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/WorkspaceManager");
        if (c) {
            jmethodID m1 = env->GetStaticMethodID(c, "rootPath", "()Ljava/lang/String;");
            jmethodID m2 = env->GetStaticMethodID(c, "statusText", "()Ljava/lang/String;");
            jmethodID m3 = env->GetStaticMethodID(c, "defaultPath", "()Ljava/lang/String;");
            if (m1) g_wsPath    = JStr(env, (jstring)env->CallStaticObjectMethod(c, m1));
            if (m2) g_wsStatus  = JStr(env, (jstring)env->CallStaticObjectMethod(c, m2));
            if (m3) g_wsDefault = JStr(env, (jstring)env->CallStaticObjectMethod(c, m3));
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    if (!g_wsEditInit) {
        snprintf(g_wsEdit, sizeof(g_wsEdit), "%s", g_wsPath.c_str());
        g_wsEditInit = true;
    }
    g_wsLoaded = true;
}

void WsSetPath(const char* p) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/WorkspaceManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "setRootPath", "(Ljava/lang/String;)V");
            if (m) {
                jstring js = env->NewStringUTF(p == nullptr ? "" : p);
                env->CallStaticVoidMethod(c, m, js);
                env->DeleteLocalRef(js);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    g_wsLoaded = false;
}

void WsPickDir() {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env && g_activityRef != nullptr) {
        jclass c = env->GetObjectClass(g_activityRef);
        if (c) {
            jmethodID m = env->GetMethodID(c, "pickWorkspaceDir", "()V");
            if (m) env->CallVoidMethod(g_activityRef, m);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
}

// 小标题 + 值 的一行
void Row(const char* label, const std::string& value, const ImVec4& valCol) {
    const Md3Palette& p = CurPalette();
    ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
    ImGui::SetWindowFontScale(0.78f);
    ImGui::TextUnformatted(label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine(170.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, valCol);
    ImGui::SetWindowFontScale(0.86f);
    ImGui::TextWrapped("%s", value.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
}

void McpCallI(const char* method, const char* sig, int i) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, method, sig);
            if (m) env->CallStaticVoidMethod(c, m, (jint)i);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
}

// 改名 / 改地址
void McpUpdate(int i, const char* name, const char* url) {
    bool att = false;
    JNIEnv* env = Attach(att);
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "update",
                    "(ILjava/lang/String;Ljava/lang/String;)V");
            if (m) {
                jstring a = env->NewStringUTF(name == nullptr ? "" : name);
                jstring b = env->NewStringUTF(url == nullptr ? "" : url);
                env->CallStaticVoidMethod(c, m, (jint)i, a, b);
                env->DeleteLocalRef(a);
                env->DeleteLocalRef(b);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    g_loaded = false;
}

// ---------- 预设 ----------
int PresetCount() {
    bool att = false;
    JNIEnv* env = Attach(att);
    int n = 0;
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "presetCount", "()I");
            if (m) n = env->CallStaticIntMethod(c, m);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    return n;
}

std::string PresetField(int i, const char* key) {
    bool att = false;
    JNIEnv* env = Attach(att);
    std::string out;
    if (env) {
        jclass c = Cls(env, "com/u3dexample/McpManager");
        if (c) {
            jmethodID m = env->GetStaticMethodID(c, "presetField",
                    "(ILjava/lang/String;)Ljava/lang/String;");
            if (m) {
                jstring k = env->NewStringUTF(key);
                jstring r = (jstring)env->CallStaticObjectMethod(c, m, (jint)i, k);
                out = JStr(env, r);
                env->DeleteLocalRef(k);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
        Detach(att);
    }
    return out;
}

const char* PresetIdC(int i) {
    static char buf[64];
    std::string s = PresetField(i, "id");
    snprintf(buf, sizeof(buf), "%s", s.c_str());
    return buf;
}

// 一个服务器条目
void Md3Item(int i) {
    const Md3Palette& p = CurPalette();
    McpItem& it = g_items[i];

    ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurface);
    ImGui::SetWindowFontScale(0.86f);
    ImGui::TextUnformatted(it.name.empty() ? "(未命名)" : it.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
    ImGui::SetWindowFontScale(0.72f);
    ImGui::TextWrapped("%s", it.url.empty() ? "（未填写地址）" : it.url.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    bool en = it.enabled;
    if (Md3Checkbox("启用", &en)) {
        McpSetEnabled(i, en);
        it.enabled = en;
    }
    ImGui::SameLine();
    if (Md3Button("编辑", ImVec2(130.0f, 0))) {
        g_editIdx = i;
        snprintf(g_editName, sizeof(g_editName), "%s", g_items[i].name.c_str());
        snprintf(g_editUrl, sizeof(g_editUrl), "%s", g_items[i].url.c_str());
        g_editOpen = true;
    }
    ImGui::SameLine();
    if (Md3Button("测试", ImVec2(140.0f, 0))) {
        g_testIdx = i;
        g_testMsg = "测试中…";
        McpCallI("testAsync", "(I)V", i);
    }
    ImGui::SameLine();
    if (Md3Button("删除", ImVec2(140.0f, 0))) {
        McpCallI("remove", "(I)V", i);
        g_loaded = false;
    }

    if (g_testIdx == i && !g_testMsg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, p.Primary);
        ImGui::SetWindowFontScale(0.74f);
        ImGui::TextWrapped("%s", g_testMsg.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, 6.0f));
}

} // namespace

// =====================================================================
//  JNI 回调：测试连接结果
// =====================================================================
extern "C" {

JNIEXPORT void JNICALL
Java_com_u3dexample_McpManager_nativeOnMcpTest(JNIEnv* env, jclass, jint index, jstring msg) {
    g_testIdx = (int)index;
    g_testMsg = JStr(env, msg);
    g_loaded  = false;   // 让下次刷新重新拉工具数
}

} // extern "C"

// =====================================================================
//  设置页：工作区 & MCP
// =====================================================================
void PageWorkspaceCard() {
    if (!g_wsLoaded) ReloadWs();
    if (!g_loaded)   ReloadMcp();

    const Md3Palette& p = CurPalette();

    // ---------------- 工作区 ----------------
    Md3Card("##card_ws", p.SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, p.OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("工作区");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        bool ok = (g_wsStatus.find("可读写") != std::string::npos);
        Row("当前路径", g_wsPath.empty() ? "—" : g_wsPath, p.OnSurface);
        Row("状态", g_wsStatus.empty() ? "—" : g_wsStatus,
            ok ? p.Tertiary : Md3(255, 180, 171));

        ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.72f);
        ImGui::TextWrapped("AI 只能读写这个目录内的文件；也可以把文件「分享」给本 App，会自动落进工作区。");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // 手输路径
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.SurfaceVariant);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        bool enter = ImGui::InputTextWithHint("##ws_path", "手动输入工作区绝对路径…",
                                             g_wsEdit, sizeof(g_wsEdit),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (enter) {
            WsSetPath(g_wsEdit);
            PushLog(LOG_INFO, "工作区已设置: %s", g_wsEdit);
        }

        {
            // 三个按钮平分一行（用 -1 + SameLine 会被 ImGui 挤成 4px，这里显式算宽度）
            float bw = (ImGui::GetContentRegionAvail().x
                        - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
            if (bw < 60.0f) bw = 60.0f;
            if (Md3Button("选择目录", ImVec2(bw, 0))) WsPickDir();
            ImGui::SameLine();
            if (Md3Button("应用输入", ImVec2(bw, 0))) WsSetPath(g_wsEdit);
            ImGui::SameLine();
            if (Md3Button("还原默认", ImVec2(bw, 0))) {
                WsSetPath(g_wsDefault.c_str());
                g_wsEditInit = false;
            }
        }
    }
    Md3CardEnd();

    // ---------------- MCP ----------------
    Md3Card("##card_mcp", p.SurfaceContainerHigh);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, p.OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("MCP 服务器");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine(200.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
        ImGui::SetWindowFontScale(0.74f);
        char cnt[64];
        snprintf(cnt, sizeof(cnt), "已配置 %d 个 · 启用后其工具会直接出现在对话里", (int)g_items.size());
        ImGui::TextUnformatted(cnt);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        if (g_items.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.80f);
            ImGui::TextWrapped("还没有服务器。点下面的「添加预设」，可直接接入 DeepWiki / Context7 等真实开源 MCP。");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < (int)g_items.size(); i++) {
            ImGui::PushID(7000 + i);
            Md3Item(i);
            ImGui::PopID();
        }

        if (Md3Button("添加预设", ImVec2(-1, 0))) ImGui::OpenPopup("##mcp_presets");

        ImGui::PushStyleColor(ImGuiCol_PopupBg, p.SurfaceContainerHigh);
        ImGui::PushStyleColor(ImGuiCol_Border, p.OutlineVariant);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        if (ImGui::BeginPopup("##mcp_presets")) {
            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurface);
            ImGui::TextUnformatted("选择要接入的 MCP");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 2.0f));
            for (int i = 0; i < PresetCount(); i++) {
                std::string nm   = PresetField(i, "name");
                std::string hint = PresetField(i, "hint");
                std::string pid  = PresetField(i, "id");
                ImGui::PushID(8000 + i);
                if (Md3Button(nm.c_str(), ImVec2(420.0f, 0))) {
                    if (pid == "custom") g_openEditForLast = true;
                    McpCallStr("addPreset", "(Ljava/lang/String;)V", PresetIdC(i));
                    g_loaded = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
                ImGui::SetWindowFontScale(0.70f);
                ImGui::TextWrapped("%s", hint.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        // ---------- 编辑弹窗（改名 / 改地址，名称与地址都可以自己填）----------
        if (g_editOpen) {
            ImGui::OpenPopup("##mcp_edit");
            g_editOpen = false;
        }
        ImGui::PushStyleColor(ImGuiCol_PopupBg, p.SurfaceContainerHigh);
        ImGui::PushStyleColor(ImGuiCol_Border, p.OutlineVariant);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.SurfaceVariant);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 18.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        if (ImGui::BeginPopupModal("##mcp_edit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurface);
            ImGui::TextUnformatted("编辑 MCP 服务器");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 2));

            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.74f);
            ImGui::TextUnformatted("名称（自己填写，例如 SSH）");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            ImGui::SetNextItemWidth(520.0f);
            ImGui::InputText("##ed_name", g_editName, sizeof(g_editName));

            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.74f);
            ImGui::TextUnformatted("地址（MCP endpoint，例如 http://127.0.0.1:8787/mcp）");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            ImGui::SetNextItemWidth(520.0f);
            ImGui::InputText("##ed_url", g_editUrl, sizeof(g_editUrl));

            ImGui::Dummy(ImVec2(0, 4));
            if (Md3Button("保存", ImVec2(520.0f, 0))) {
                McpUpdate(g_editIdx, g_editName, g_editUrl);
                g_testIdx = -1;
                g_testMsg.clear();
                ImGui::CloseCurrentPopup();
            }
            {
                ImGui::PushStyleColor(ImGuiCol_Button, p.SurfaceVariant);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.SurfaceVariant);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.SurfaceContainer);
                ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));
                if (ImGui::Button("取消", ImVec2(520.0f, 0))) ImGui::CloseCurrentPopup();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
    }
    Md3CardEnd();
}
