// md3_i18n.cpp - Internationalization string tables and language helpers
#include "md3_common.h"
#include <cstring>

// ================= 全量中英基础设施 =================
int g_lang = 0;

const char* TL(const char* zh, const char* en) { return (g_setLang == 1) ? en : zh; }

// 词条字典: 没收录的词条会原样返回中文, 不会出现空白
static const struct { const char* zh; const char* en; } kTerms[] = {
    {"主页","Home"},{"战斗","Battle"},{"世界","World"},{"终端","Terminal"},
    {"设置","Settings"},{"绘制","Visuals"},{"美化","Skins"},{"立绘","Live2D"},
    {"运动模糊","Motion blur"},{"立绘语音","Voice"},{"卡密验证","License"},
    {"显示范围圈","Show range circle"},{"自瞄范围圈","Aim range"},{"范围半径","Radius"},
    {"圆圈颜色","Circle colour"},{"细线条","Thin line"},
    {"总开关","Master switch"},{"绘制 ID","Draw 棍母"},{"显示距离","Distance"},
    {"方框","Box"},{"骨骼","Skeleton"},{"血条","Health bar"},{"射线","Snapline"},
    {"方框样式","Box style"},{"方框粗细","Box thickness"},{"绘制颜色","Colours"},
    {"绘制预览","Preview"},{"仅「绘制」页显示 · 随界面折叠","Only on Visuals · folds with UI"},
    {"2D 方框","2D box"},{"3D 方框","3D box"},{"霓虹框","Neon box"},
    {"模型缩放","Model scale"},{"头部跟随幅度","Head tracking"},{"触摸跟随视线","Gaze follow"},
    {"自动眨眼","Auto blink"},{"模拟说话","Simulate talk"},{"怠速","Idle"},{"轻击","Tap"},
    {"表情","Expression"},{"试听","Preview voice"},{"萝莉音","Loli voice"},
    {"音调","Pitch"},{"语速","Speed"},{"AI 回复时语音播报","Speak AI replies"},
    {"启用运动模糊","Enable motion blur"},{"模糊力度","Blur strength"},
    {"显示立绘","Show Live2D"},{"立即恢复默认","Restore defaults"},
    {"AI 控制台","AI console"},{"语言","Language"},{"中文","Chinese"},{"English","English"},
    {"流星雨","Meteor shower"},{"深度黑暗","Dark"},{"浅色","Light"},
    {"卡密","License key"},{"解锁","Unlock"},{"解密卡密","Decode key"},{"购买卡密","Buy a key"},
    {"刷新","Refresh"},{"已就绪","Ready"},{"未使用","Unused"},
    {"网络与位置","Network & location"},{"公网 IP","Public IP"},{"归属地","Region"},
    {"运营商","ISP"},{"经纬度","Lat/Lon"},{"时区","Time zone"},
};
static const int kTermCount = (int)(sizeof(kTerms) / sizeof(kTerms[0]));

const char* TR(const char* zh) {
    if (zh == nullptr) return "";
    if (g_setLang != 1) return zh;
    for (int i = 0; i < kTermCount; ++i)
        if (strcmp(kTerms[i].zh, zh) == 0) return kTerms[i].en;
    return zh;   // 未收录: 保持原样式, 不出现空白
}


int g_setLang = 0;

static const char* kStr[2][SK_COUNT] = {
{
    "主页","战斗","世界","终端","设置","绘制",
    "主页","欢迎使用 Avates。这里展示了所有可用的 UI 控件。",
    "输入与按钮","输入你的名字...","打个招呼","计数器","计数: %d",
    "滑块与进度条","进度 %.0f%%",
    "选择控件","启用通知","自动保存","选项 %c","低\0中\0高\0\0",
    "战斗","战斗模块配置。调整参数后实时生效。",
    "总开关","启用战斗模块","自动模式",
    "参数调节","攻击范围","%.1f 格","攻击速度","%.1f 次/秒",
    "视野 FOV","近战\0远程\0混合\0\0",
    "运行状态","模块: %s","运行中","已停止",
    "模式: %s  自动: %s","是","否",
    "世界","网络连接与服务器配置。",
    "服务器配置","服务器地址","端口 %d",
    "断开连接","连接服务器",
    "连接状态","状态: %s","已连接","未连接",
    "地址: %s:%d","协议: %s",
    "网络延迟","未连接,无法获取延迟。",
    "日志","项目运行日志(实时追加,最新在底部)。",
    "设置","应用配置与个性化。",
    "通用","开启通知","开机自启","触觉反馈","亮度 %.0f%%",
    "音量","音量 %.0f%%",
    "外观","配色方案","明暗模式","跟随系统\0浅色\0深色\0\0",
    "语言","简体中文\0English\0\0",
    "蓝色\0绿色\0橙色\0红色\0紫色\0\0",
},
{
    "Home","Battle","World","Terminal","Settings","Visuals",
    "Home","Welcome to Avates. All available UI controls are shown here.",
    "Input & Buttons","Enter your name...","Say Hello","Counter","Count: %d",
    "Sliders & Progress","Progress %.0f%%",
    "Selection","Enable Notifications","Auto Save","Option %c","Low\0Medium\0High\0\0",
    "Battle","Battle module configuration. Adjust parameters for real-time effect.",
    "Master Switch","Enable Battle Module","Auto Mode",
    "Parameters","Attack Range","%.1f blocks","Attack Speed","%.1f/s",
    "View FOV","Melee\0Ranged\0Hybrid\0\0",
    "Status","Module: %s","Running","Stopped",
    "Mode: %s  Auto: %s","Yes","No",
    "World","Network connection and server configuration.",
    "Server Config","Server Address","Port %d",
    "Disconnect","Connect",
    "Connection","Status: %s","Connected","Not Connected",
    "Address: %s:%d","Protocol: %s",
    "Latency","Not connected, cannot get latency.",
    "Logs","Runtime logs (appended in real-time, newest at bottom).",
    "Settings","App configuration and personalization.",
    "General","Notifications","Auto Start","Haptic Feedback","Brightness %.0f%%",
    "Volume","Volume %.0f%%",
    "Appearance","Color Scheme","Dark/Light Mode","Follow System\0Light\0Dark\0\0",
    "Language","简体中文\0English\0\0",
    "Blue\0Green\0Orange\0Red\0Purple\0\0",
}
};

const char* T(StrKey k) { return kStr[g_setLang][k]; }

const char* ThemeName(int idx) {
    const char* p = T(SK_COMBO_COLOR_SCHEME);
    for (int i = 0; i < idx && *p; i++) {
        while (*p && *p != '\0') p++;
        if (*p == '\0') p++;
    }
    return p;
}

const char* NavLabel(int i) {
    switch (i) {
    case NAV_HOME:     return T(SK_NAV_HOME);
    case NAV_TARGET:   return T(SK_NAV_BATTLE);
    case NAV_DRAW:     return T(SK_NAV_DRAW);
    case NAV_GLOBE:    return T(SK_NAV_WORLD);
    case NAV_TERMINAL: return T(SK_NAV_TERMINAL);
    case NAV_SETTINGS: return T(SK_NAV_SETTINGS);
    }
    return "";
}
