// md3_common.h - MD3 UI shared types, globals, and function declarations
#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <GLES2/gl2.h>
#include <jni.h>
#include <atomic>
#include <cstdarg>
#include <cmath>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "u3d-imgui", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "u3d-imgui", __VA_ARGS__)

static const char* FONT_FILE = "SourceHanSansCN-Bold.otf";
static const float FONT_SIZE = 32.0f;

static const ImWchar GLYPH_RANGES[] = {
    0x0020, 0x00FF,
    0x2000, 0x206F,
    0x3000, 0x303F,
    0xFF00, 0xFFEF,
    0x3400, 0x4DBF,
    0x4E00, 0x9FFF,
    0,
};

struct TouchEvent { int action; float x, y; };
enum InputEventType { EV_CHAR, EV_BACKSPACE };
struct InputEvent { int type; unsigned int ch; };
enum LogLevel { LOG_INIT, LOG_INFO, LOG_OK, LOG_WARN, LOG_ERR };

enum ThemeScheme { SCHEME_BLUE = 0, SCHEME_GREEN, SCHEME_ORANGE, SCHEME_RED, SCHEME_PURPLE, THEME_COUNT };

struct Md3Palette {
    ImVec4 Primary, OnPrimary, PrimaryContainer, OnPrimaryContainer;
    ImVec4 Secondary, SecondaryContainer;
    ImVec4 Surface, SurfaceContainer, SurfaceContainerHigh, SurfaceVariant;
    ImVec4 OnSurface, OnSurfaceVariant;
    ImVec4 Outline, OutlineVariant;
    ImVec4 Tertiary, TertiaryContainer;
};

enum NavItem { NAV_HOME = 0, NAV_TARGET, NAV_DRAW, NAV_GLOBE, NAV_TERMINAL, NAV_SETTINGS, NAV_COUNT };

enum StrKey {
    SK_NAV_HOME, SK_NAV_BATTLE, SK_NAV_WORLD, SK_NAV_TERMINAL, SK_NAV_SETTINGS, SK_NAV_DRAW,
    SK_HOME_TITLE, SK_HOME_SUB,
    SK_INPUT_BUTTONS, SK_INPUT_NAME_HINT, SK_SAY_HELLO, SK_COUNTER, SK_COUNT_FMT,
    SK_SLIDER_PROGRESS, SK_PROGRESS_FMT,
    SK_SELECTION, SK_ENABLE_NOTIFY, SK_AUTO_SAVE, SK_OPTION, SK_COMBO_LMH,
    SK_BATTLE_TITLE, SK_BATTLE_SUB,
    SK_MASTER_SWITCH, SK_ENABLE_BATTLE, SK_AUTO_MODE,
    SK_PARAMS, SK_ATK_RANGE, SK_ATK_RANGE_FMT, SK_ATK_SPEED, SK_ATK_SPEED_FMT,
    SK_VIEW_FOV, SK_COMBO_MELEE,
    SK_RUN_STATUS, SK_MODULE_FMT, SK_RUNNING, SK_STOPPED,
    SK_MODE_AUTO_FMT, SK_YES, SK_NO,
    SK_WORLD_TITLE, SK_WORLD_SUB,
    SK_SERVER_CFG, SK_SERVER_ADDR_HINT, SK_PORT_FMT,
    SK_DISCONNECT, SK_CONNECT,
    SK_CONN_STATUS, SK_STATUS_FMT, SK_CONNECTED, SK_NOT_CONNECTED,
    SK_ADDR_FMT, SK_PROTOCOL_FMT,
    SK_LATENCY, SK_NO_LATENCY,
    SK_LOG_TITLE, SK_LOG_SUB,
    SK_SETTINGS_TITLE, SK_SETTINGS_SUB,
    SK_GENERAL, SK_NOTIFY, SK_AUTO_START, SK_HAPTIC, SK_BRIGHTNESS_FMT,
    SK_VOLUME, SK_VOLUME_FMT,
    SK_APPEARANCE, SK_COLOR_SCHEME, SK_DARK_LIGHT, SK_COMBO_THEME,
    SK_LANGUAGE, SK_COMBO_LANG,
    SK_COMBO_COLOR_SCHEME,
    SK_COUNT
};

extern const Md3Palette kPalettes[THEME_COUNT][2];
extern int g_themeScheme;
extern bool g_darkMode;
extern int g_setLang;
extern int g_navIndex;
extern float g_indicatorY;
extern float g_indicatorVelY;
extern float g_itemSwitchAnim;
extern bool g_indicatorInit;
extern int g_cardCounter;
extern ANativeWindow* g_window;
extern std::mutex g_queueMutex;
extern std::deque<TouchEvent> g_touchQueue;
extern std::deque<InputEvent> g_inputQueue;
extern std::atomic<bool> g_initialized;
extern std::vector<char> g_fontBytes;
extern jobject g_activityRef;
extern JavaVM* g_jvm;
extern jmethodID g_showImeMethod;
extern jmethodID g_hideImeMethod;

extern char g_homeBuf[64];
extern char g_termBuf[128];
extern int g_setChoice;
extern bool g_setNotify;
extern float g_setVolume;
extern float g_progressDemo;
extern int g_radioDemo;
extern bool g_checkDemo1;
extern bool g_checkDemo2;
extern int g_comboDemo;
extern float g_sliderDemo;
extern int g_counterDemo;
extern bool g_combatEnabled;
extern float g_combatRange;
extern float g_combatSpeed;
extern int g_combatMode;
extern bool g_combatAuto;
extern float g_combatFOV;
extern char g_worldAddr[64];
extern int g_worldPort;
extern bool g_worldConnected;
extern float g_worldLatency;
extern int g_worldProtocol;
extern bool g_setAutoStart;
extern bool g_setHaptic;
extern float g_setBrightness;

ImVec4 Md3(float r, float g, float b, float a = 1.0f);
const Md3Palette& CurPalette();
ImU32 Md3U32(const ImVec4& c);
void SetupMD3Theme();
const char* T(StrKey k);
const char* ThemeName(int idx);
const char* NavLabel(int i);
void SpringUpdate(float& pos, float& vel, float target, float dt, float omega = 14.0f, float zeta = 0.55f);
float EaseOutCubic(float t);
float EaseOutBack(float t);
float AnimateSpring(ImGuiID id, bool target, float dt);
void HandleTouchDragScroll(bool includeChildWindows = true);

// ---- md3_motionblur.cpp : 全 UI 运动模糊（帧反馈拖影）----
namespace MotionBlur {
    bool  Enabled();
    float Strength();
    void  SetEnabled(bool v);
    void  SetStrength(float v);
    void  SetScreenSize(int w, int h);
    void  BeginCapture();
    void  EndCaptureAndPresent();
    const char* StatusText();
    bool  Capturing();
    void  OnContextLost();
}
// ---- md3_live2d.cpp : 立绘 (Live2D Cubism, 内嵌数据方案照 ImGuiLive2d-main) ----
namespace Live2D {
    bool  Show();
    void  SetShow(bool v);
    bool  Ready();
    float Scale();
    void  SetScale(float v);
    float HeadGain();
    void  SetHeadGain(float v);
    bool  GazeFollow();
    void  SetGazeFollow(bool v);
    bool  AutoBlink();
    void  SetAutoBlink(bool v);
    bool  Talking();
    void  SetTalkManual(bool v);
    bool  TalkManual();
    void  SetTalking(bool v);
    const char* StatusText();
    GLuint Tex();
    bool   BoundsOk();
    void   BoundsNorm(float* x0, float* y0, float* x1, float* y1);
    void   LastCanvasRect(ImVec2* a, ImVec2* b);
    void   GazeAt(float nx, float ny);
    void  TriggerIdle();
    void  TriggerTap();
    void  TriggerExpression();
    void  SetThinking(bool v);
    bool  Thinking();
    void  OnFrame();
    void  OnContextLost();
    void  DrawStandPanel(float px, float py, float pw, float ph);
}


// ---- md3_tts.cpp : 立绘语音播报 ----
namespace Tts {
    void  Init();
    void  Say(const std::string& text);
    void  Stop();
    bool  Speaking();
    bool  Enabled();
    void  SetEnabled(bool v);
    bool  Loli();
    void  SetLoli(bool v);
    float Pitch();
    void  SetPitch(float v);
    float Rate();
    void  SetRate(float v);
}

bool AiIsStreaming();
extern "C" void AiTtsFinish(bool speakRest);   // 一轮AI结束: 收尾最后半句

// ---- 语言: 0=中文 1=English ----
extern int g_lang;
const char* TL(const char* zh, const char* en);
const char* TR(const char* zh);

namespace Fx {
    bool On();
    void SetOn(bool v);
    void Update(float dt, float W, float H);
    void Draw(ImDrawList* fg, float W, float H);
}
namespace Lic {
    bool  Unlocked();
    void  SetLocked(bool v);
    void  NotifyWrong();
    float ShakeAmp();
    float ErrAmount();
    void  Update(float dt);
    void  DrawPage();
    void  ApplyShake(ImDrawData* dd);
    void  OpenUrl(const char* url);
}
namespace UiCmd {
    bool Exec(const std::string& key, const std::string& val);
    int  ScanAndExec(const std::string& text);
    void RestoreAll();
}

void SetNavIndex(int i);
int  NavIndexOf();
void EspSetMaster(bool on);
bool EspMaster();
void SetDarkMode(bool dark);
bool DarkModeOn();
void PushLog(LogLevel lv, const char* fmt, ...);
void DrawIconHome(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void DrawIconTarget(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void DrawIconGlobe(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void DrawIconTerminal(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void DrawIconSettings(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void DrawIconDraw(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool selected);
void Md3Card(const char* id, const ImVec4& bg);
void Md3CardEnd();
void PageHeader(const char* title, const char* subtitle);
bool Md3Checkbox(const char* label, bool* v);
bool Md3RadioButton(const char* label, int* v, int v_button);
bool Md3SliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt);
bool Md3SliderInt(const char* label, int* v, int v_min, int v_max, const char* fmt);
bool Md3Button(const char* label, const ImVec2& size_arg);
bool Md3Combo(const char* label, int* current_item, const char* items_separated_by_zeros);

// ---------- 新增控件 (二级导航 + 美化卡片) ----------
void Md3SubNav(const char* id, const char* const* items, int count, int* current);
/**
 * 美化卡片: 名称 / 皮肤副标题 / 序号-总数数字步进器 / - + / 【美化】动作按钮
 *   id        —— ImGui 唯一 id
 *   name      —— 名称文本 (例如枪械原名)
 *   skins     —— 皮肤名字数组
 *   skinCount —— 皮肤数量
 *   idx       —— 当前序号 (0 基, 界面显示 1 基)
 * 返回 true 表示按下了「美化」。
 */
bool Md3BeautifyCard(const char* id, const char* name, const char* const* skins,
                     int skinCount, int* idx);
void PageHome();
void PageTarget();
void PageGlobe();
void PageTerminal();
void PageSettings();
void PageAiChat();
void PageNetCard();
void PageWorkspaceCard();
bool AiIsBusy();
void RenderDemoFrame();
void DrainQueues(ImGuiIO& io);
void SetPlatformImeDataFn(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData* data);
void CleanupImGui();
bool LoadAsset(AAssetManager* am, const char* name, std::vector<char>& out);

// ---------------------------------------------------------------------
//  UI 动效 (md3_uianim.cpp): 音量键驱动的「折叠隐藏 / 渐显显示」
// ---------------------------------------------------------------------
namespace UiAnim {
void Hide();                                   // 折叠隐藏 (音量-)
void Show();                                   // 渐显显示 (音量+)
void Toggle();
void Update(float dt);                         // 每帧推进动画
void OnVolumeKey(int keyCode, bool down);      // 24=音量+, 25=音量-
bool HardHidden();
void TransformDrawData(ImDrawData* dd);        // 必须在 ImGui::Render() 之后调用
bool Hidden();
void SetKeepVisible(float x, float y, float w, float h);
bool HitKeepVisible(float x, float y);
void ClearKeepVisible();
void SetKeepRing(float cx, float cy, float r, float tol);
void SetKeepVerts(int slot, const void* drawList, int i0, int i1);
void ClearKeepVerts(int slot);
void IconHide(float anchorX, float anchorY);
void IconShow(float anchorX, float anchorY);
bool InIconAnim();
bool IconShown();
bool IconAnimShowing();
float IconAnimProgress();
bool FoldMode();
void  SetDotsHit(float a, float b, float c, float d);
void  SetIconHit(float a, float b, float c, float d);
void  SetIconAnchor(float x, float y);
bool  HitDots(float x, float y);
bool  HitIcon(float x, float y);
float IconAnchorX();
float IconAnchorY();
void  NudgeIcon(float dx, float dy);
void  TakeNudge(float& dx, float& dy);
void  RequestShow();
bool  TakeRequestShow();
bool BlocksInput();
}

