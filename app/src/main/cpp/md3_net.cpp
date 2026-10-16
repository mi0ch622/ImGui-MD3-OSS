// md3_net.cpp - 「网络与位置」状态卡片
//   * VPN 是否开启（连通性判定 + 虚拟网卡兜底，由 Java 侧算好）
//   * 公网 IP / 详细归属地（国家·省·市 / 运营商 ASN / 经纬度 / 邮编 / 时区）
//   * 内网 IP
//   * 数据来自 NetInfoBridge（ipwho.is 主源、ipinfo.io 备源）
#include "md3_common.h"
#include <cstdio>
#include <cstring>

namespace {

std::mutex  g_netMtx;
bool        g_netLoaded  = false;   // 是否已拿到过一次结果
bool        g_netBusy    = false;   // Java 侧正在查询
bool        g_netVpn     = false;
std::string g_netLocalIp, g_netPublicIp, g_netCountry, g_netRegion, g_netCity;
std::string g_netIsp, g_netLat, g_netLon, g_netTz, g_netPostal;
std::string g_netSource, g_netError;

std::string TrimStr(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

// 极简 JSON 取值（只处理扁平字符串/数字字段，够用且不引入额外依赖）
std::string JsonStr(const std::string& j, const char* key) {
    std::string pat = std::string("\"") + key + "\"";
    size_t p = j.find(pat);
    if (p == std::string::npos) return std::string();
    p = j.find(':', p + pat.size());
    if (p == std::string::npos) return std::string();
    ++p;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) ++p;
    if (p >= j.size()) return std::string();
    if (j[p] == '"') {
        ++p;
        std::string out;
        while (p < j.size() && j[p] != '"') {
            if (j[p] == '\\' && p + 1 < j.size()) {
                char c = j[p + 1];
                if (c == 'n') out += '\n';
                else if (c == 't') out += '\t';
                else if (c == 'r') out += '\r';
                else if (c == 'u') { p += 6; continue; }
                else out += c;
                p += 2;
                continue;
            }
            out += j[p++];
        }
        return out;
    }
    std::string out;
    while (p < j.size() && j[p] != ',' && j[p] != '}' && j[p] != ']') out += j[p++];
    return TrimStr(out);
}

// 调用 Java 侧的 NetInfoBridge.refresh()
void NetInfoDoRefresh() {
    if (g_jvm == nullptr) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    if (env != nullptr) {
        jclass cls = env->FindClass("com/u3dexample/NetInfoBridge");
        if (cls != nullptr) {
            jmethodID mid = env->GetStaticMethodID(cls, "refresh", "()V");
            if (mid != nullptr) env->CallStaticVoidMethod(cls, mid);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(cls);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
    if (attached) g_jvm->DetachCurrentThread();
}

// 一行「标签 — 值」
void NetRow(const char* label, const std::string& value) {
    const Md3Palette& p = CurPalette();
    ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
    ImGui::SetWindowFontScale(0.78f);
    ImGui::TextUnformatted(label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(190.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurface);
    ImGui::SetWindowFontScale(0.86f);
    ImGui::TextWrapped("%s", value.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
}

std::string Join3(const std::string& a, const std::string& b, const std::string& c) {
    std::string s = a;
    if (!b.empty()) s += (s.empty() ? "" : " · ") + b;
    if (!c.empty()) s += (s.empty() ? "" : " · ") + c;
    return s;
}

} // namespace

// =====================================================================
//  JNI 回调
// =====================================================================
extern "C" {

JNIEXPORT void JNICALL
Java_com_u3dexample_NetInfoBridge_nativeOnNetInfo(JNIEnv* env, jclass, jstring json) {
    if (json == nullptr) return;
    const char* c = env->GetStringUTFChars(json, nullptr);
    if (c == nullptr) return;
    std::string j = c;
    env->ReleaseStringUTFChars(json, c);

    std::lock_guard<std::mutex> lk(g_netMtx);
    g_netVpn      = (JsonStr(j, "vpn") == "1");
    g_netLocalIp  = JsonStr(j, "localIp");
    g_netPublicIp = JsonStr(j, "publicIp");
    g_netCountry  = JsonStr(j, "country");
    g_netRegion   = JsonStr(j, "region");
    g_netCity     = JsonStr(j, "city");
    g_netIsp      = JsonStr(j, "isp");
    g_netLat      = JsonStr(j, "lat");
    g_netLon      = JsonStr(j, "lon");
    g_netTz       = JsonStr(j, "tz");
    g_netPostal   = JsonStr(j, "postal");
    g_netSource   = JsonStr(j, "source");
    g_netError    = JsonStr(j, "error");
    g_netLoaded   = true;
    g_netBusy     = false;
}

} // extern "C"

// =====================================================================
//  卡片
// =====================================================================
void PageNetCard() {
    // 首次进入主页时自动查一次（放在加锁之前，避免回调抢锁）
    static bool s_requested = false;
    {
        std::lock_guard<std::mutex> lk(g_netMtx);
        if (!s_requested && !g_netBusy) {
            s_requested = true;
            g_netBusy = true;
            NetInfoDoRefresh();
        }
    }

    std::lock_guard<std::mutex> lk(g_netMtx);

    const Md3Palette& p = CurPalette();
    const ImVec4 errCol = Md3(255, 180, 171);   // MD3 error 色（深色）
    Md3Card("##card_net", p.SurfaceContainerHigh);
    {
        // ---------- 标题行 ----------
        ImGui::PushStyleColor(ImGuiCol_Text, p.OnPrimaryContainer);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted("网络与位置");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine(190.0f);
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 tp = ImGui::GetCursorScreenPos();
            float lineH = ImGui::GetTextLineHeight();

            bool loading = g_netBusy && !g_netLoaded;
            ImVec4 dotCol = loading ? p.Outline
                                    : (g_netError.empty() ? p.Tertiary : errCol);
            dl->AddCircleFilled(ImVec2(tp.x + 8.0f, tp.y + lineH * 0.5f), 6.0f, Md3U32(dotCol));
            ImGui::Dummy(ImVec2(20.0f, 0.0f));
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.74f);
            ImGui::TextUnformatted(loading ? "正在查询…"
                                           : (g_netError.empty() ? "已就绪" : "查询异常"));
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (Md3Button("刷新", ImVec2(120.0f, 0))) {
                g_netBusy = true;
                g_netError.clear();
                NetInfoDoRefresh();
            }
        }

        // ---------- VPN ----------
        {
            ImGui::PushStyleColor(ImGuiCol_Text, p.OnSurfaceVariant);
            ImGui::SetWindowFontScale(0.78f);
            ImGui::TextUnformatted("VPN 状态");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SameLine(190.0f);
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 tp = ImGui::GetCursorScreenPos();
                float lineH = ImGui::GetTextLineHeight();
                dl->AddCircleFilled(ImVec2(tp.x + 8.0f, tp.y + lineH * 0.5f), 6.0f,
                                    Md3U32(g_netVpn ? p.Primary : p.OutlineVariant));
                ImGui::Dummy(ImVec2(20.0f, 0.0f));
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, g_netVpn ? p.Primary : p.OnSurfaceVariant);
                ImGui::SetWindowFontScale(0.86f);
                ImGui::TextUnformatted(g_netVpn ? "已开启（流量经 VPN 转发）" : "未使用");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
            }
        }

        if (!g_netError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, errCol);
            ImGui::SetWindowFontScale(0.80f);
            ImGui::TextWrapped("%s", g_netError.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        // ---------- 公网信息 ----------
        NetRow("公网 IP", g_netPublicIp.empty() ? "—" : g_netPublicIp);
        NetRow("归属地", Join3(g_netCountry, g_netRegion, g_netCity).empty()
                             ? "—" : Join3(g_netCountry, g_netRegion, g_netCity));
        NetRow("运营商", g_netIsp.empty() ? "—" : g_netIsp);
        {
            std::string geo = "—";
            if (!g_netLat.empty() && !g_netLon.empty()) {
                char b[96];
                snprintf(b, sizeof(b), "%s, %s", g_netLat.c_str(), g_netLon.c_str());
                geo = b;
            }
            NetRow("经纬度", geo);
        }
        NetRow("时区", g_netTz.empty() ? "—" : g_netTz);
        NetRow("邮编", g_netPostal.empty() ? "—" : g_netPostal);
        NetRow("内网 IP", g_netLocalIp.empty() ? "—" : g_netLocalIp);

        // ---------- 来源 ----------
        {
            ImGui::PushStyleColor(ImGuiCol_Text, p.Outline);
            ImGui::SetWindowFontScale(0.68f);
            std::string src = "数据源：";
            src += g_netSource.empty() ? "—" : g_netSource;
            ImGui::TextUnformatted(src.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }
    }
    Md3CardEnd();
}