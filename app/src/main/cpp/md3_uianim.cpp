// md3_uianim.cpp —— 音量键驱动的 UI 动效（折叠隐藏 / 渐显显示）
//
// 音量 -  :整个界面**按行折叠**到屏幕顶部（顶部先折，向下逐行跟上），并随折叠淡出
// 音量 +  :iOS 那种**渐显**展开（先渐显，再顺带把折痕展开）
//
// 实现要点：
//   不去动任何窗口布局（Pos/Size 一律不碰），而是在 ImGui::Render() 之后、
//   交给后端渲染之前，直接对 ImDrawData 的顶点缓冲做变换：
//     * 每行按"折叠进度"压缩 y（顶部先折）→ 视觉上就是一张纸向上折起来
//     * x 轻微向中心收拢 → 增加"纸张折起"的厚度感
//     * 顶点色的 alpha 整体乘一个系数 → 渐显 / 淡出
//   这样做的好处：所有页面、所有控件、弹窗、光标，全都一次生效，零遗漏。

#include "md3_common.h"
#include <cmath>

namespace UiAnim {

static const float kHideDur = 0.20f;
static const float kShowDur = 0.16f;
static const float kIconHideDur = 0.36f;   // 吸进图标时长
static const float kIconShowDur = 0.44f;   // 从图标弹出时长

//   MD_FOLD = 音量键: 纸张折叠 + 淡出
//   MD_ICON = 点三个点: 整屏向圆角图标收缩 + 旋转 + 淡出; 展开时从图标弹出(带回弹)
enum Mode { MD_FOLD, MD_ICON };
static Mode  s_mode = MD_FOLD;
static float s_iconX = 0.0f, s_iconY = 0.0f;
static float s_scaleK = 0.0f;

enum State { ST_SHOWN, ST_HIDING, ST_HIDDEN, ST_SHOWING };

static State s_state  = ST_SHOWN;
static bool  s_hardHide = false;
static float s_t      = 0.0f;          // 当前段进度 0..1
static float s_fold   = 0.0f;          // 0=完全展开, 1=完全折起
static float s_alpha  = 1.0f;          // 不透明度
static float s_foldFrom  = 0.0f;       // 本段起始值（支持打断续演）
static float s_alphaFrom = 1.0f;
static bool  s_keepOn = false;
static float s_keepCx = 0.0f, s_keepCy = 0.0f, s_keepR = 0.0f, s_keepTol = 0.0f;

//   用圆环而不是方框, 才不会把方框内的其它 UI 内容一起保留(否则折叠残留) 
void SetKeepRing(float cx, float cy, float r, float tol) {
    s_keepOn = true; s_keepCx = cx; s_keepCy = cy; s_keepR = r; s_keepTol = tol;
}

void SetKeepVisible(float x, float y, float w, float h) {
    const float m = (w < h ? w : h);
    SetKeepRing(x + w * 0.5f, y + h * 0.5f, (w + h) * 0.25f, m * 0.5f);
}

//   slot0 保留但不再使用(范围圈已回归原样, 不做任何豁免)
struct KeepReg { const void* list; int i0; int i1; };
static KeepReg s_keep[2] = { {nullptr,0,0}, {nullptr,0,0} };
void SetKeepVerts(int slot, const void* drawList, int i0, int i1) {
    if (slot < 0 || slot > 1) return;
    s_keep[slot].list = drawList; s_keep[slot].i0 = i0; s_keep[slot].i1 = i1;
}

void ClearKeepVerts(int slot) {
    if (slot < 0 || slot > 1) return;
    s_keep[slot].list = nullptr; s_keep[slot].i0 = 0; s_keep[slot].i1 = 0;
}

static bool KeepHit(int slot, const void* cl, int idx) {
    const KeepReg& k = s_keep[slot];
    return k.list != nullptr && k.list == cl && idx >= k.i0 && idx < k.i1;
}

void ClearKeepVisible() { s_keepOn = false; }

bool HitKeepVisible(float x, float y) {
    if (!s_keepOn) return false;
    const float dx = x - s_keepCx, dy = y - s_keepCy;
    const float d2 = dx * dx + dy * dy;
    const float rout = s_keepR + s_keepTol;
    return d2 <= rout * rout;
}

static float EaseInOut(float x) { return x * x * (3.0f - 2.0f * x); }      // smoothstep
static float EaseOut(float x)   { const float u = 1.0f - x; return 1.0f - u * u * u; }

void Hide() {
    s_hardHide = true;
    if (s_state == ST_HIDING || s_state == ST_HIDDEN) return;
    s_mode = MD_FOLD;
    s_foldFrom = s_fold;
    s_alphaFrom = s_alpha;
    s_state = ST_HIDING;
    s_t = 0.0f;
}

void Show() {
    s_hardHide = false;
    if (s_state == ST_SHOWING || s_state == ST_SHOWN) return;
    s_mode = MD_FOLD;
    s_foldFrom = s_fold;
    s_alphaFrom = s_alpha;
    s_state = ST_SHOWING;
    s_t = 0.0f;
}

// 点三个点: 以图标为锚点把 UI 吸进去
void IconHide(float anchorX, float anchorY) {
    if (s_state == ST_HIDING || s_state == ST_HIDDEN) return;
    s_mode = MD_ICON; s_iconX = anchorX; s_iconY = anchorY;
    s_foldFrom = s_scaleK; s_alphaFrom = s_alpha;
    s_t = 0.0f; s_state = ST_HIDING;
}

// 点图标: 从图标位置把 UI 弹出来 (带回弹过冲)
void IconShow(float anchorX, float anchorY) {
    if (s_state == ST_SHOWING || s_state == ST_SHOWN) return;
    if (s_mode != MD_ICON) return;
    s_mode = MD_ICON; s_iconX = anchorX; s_iconY = anchorY;
    s_foldFrom = s_scaleK; s_alphaFrom = s_alpha;
    s_t = 0.0f; s_state = ST_SHOWING;
}

bool InIconAnim()       { return s_mode == MD_ICON && (s_state == ST_HIDING || s_state == ST_SHOWING); }
// (这条是“音量减隐藏后又自己冒出来”的根因修复)

static float s_dotsHit[4] = {0, 0, -1, -1};
static float s_iconHit[4] = {0, 0, -1, -1};
static float s_iconAnchor[2] = {0, 0};
static float s_nudge[2] = {0, 0};
static bool  s_pendShow = false;

void SetDotsHit(float a, float b, float c, float d) { s_dotsHit[0]=a; s_dotsHit[1]=b; s_dotsHit[2]=c; s_dotsHit[3]=d; }
void SetIconHit(float a, float b, float c, float d) { s_iconHit[0]=a; s_iconHit[1]=b; s_iconHit[2]=c; s_iconHit[3]=d; }
void SetIconAnchor(float x, float y) { s_iconAnchor[0]=x; s_iconAnchor[1]=y; }
static bool InRect(const float* r, float x, float y) {
    return r[2] > r[0] && r[3] > r[1] && x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3];
}
bool  HitDots(float x, float y) { return InRect(s_dotsHit, x, y); }
bool  HitIcon(float x, float y) { return InRect(s_iconHit, x, y); }
float IconAnchorX() { return s_iconAnchor[0]; }
float IconAnchorY() { return s_iconAnchor[1]; }
void  NudgeIcon(float dx, float dy) { s_nudge[0] += dx; s_nudge[1] += dy; }
void  TakeNudge(float& dx, float& dy) { dx = s_nudge[0]; dy = s_nudge[1]; s_nudge[0]=0; s_nudge[1]=0; }
void  RequestShow() { s_pendShow = true; }
bool  TakeRequestShow() { const bool v = s_pendShow; s_pendShow = false; return v; }
bool IconShown()        { return s_mode == MD_ICON && s_state == ST_HIDDEN; }
bool IconAnimShowing()  { return s_mode == MD_ICON && s_state == ST_SHOWING; }
float IconAnimProgress() { return s_t; }
bool FoldMode()         { return s_mode == MD_FOLD; }

void Toggle() {
    if (s_state == ST_HIDDEN || s_state == ST_HIDING) Show();
    else Hide();
}

void Update(float dt) {
    if (s_hardHide && (s_state == ST_SHOWN || s_state == ST_SHOWING)) {
        s_mode = MD_FOLD; s_state = ST_HIDDEN;
        s_fold = 1.0f; s_alpha = 0.0f; s_t = 1.0f;
        return;
    }
    if (s_state == ST_SHOWN || s_state == ST_HIDDEN) return;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    const float dur = (s_mode == MD_ICON)
        ? (s_state == ST_HIDING ? kIconHideDur : kIconShowDur)
        : (s_state == ST_HIDING ? kHideDur     : kShowDur);
    s_t += dt / dur;
    if (s_t >= 1.0f) { s_t = 1.0f; s_state = (s_state == ST_HIDING) ? ST_HIDDEN : ST_SHOWN; }

    if (s_mode == MD_FOLD) {
        if (s_state == ST_HIDING) {
            const float k = EaseOut(s_t);
            s_fold  = s_foldFrom + (1.0f - s_foldFrom) * k;
            s_alpha = s_alphaFrom * (1.0f - k);
        } else {
            const float k = EaseOut(s_t);
            s_fold  = s_foldFrom * (1.0f - k);
            s_alpha = s_alphaFrom + (1.0f - s_alphaFrom) * k;
        }
    } else {
        if (s_state == ST_HIDING) {
            const float u = s_t;
            const float k  = u * u * u;
            const float ka = u * u;
            s_scaleK = s_foldFrom + (1.0f - s_foldFrom) * k;
            s_alpha  = s_alphaFrom * (1.0f - ka);
        } else {
            const float u = s_t;
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            const float v = u - 1.0f;
            const float k = 1.0f + c3 * v * v * v + c1 * v * v;
            s_scaleK = s_foldFrom * (1.0f - k);
            float fa = u * 2.0f; if (fa > 1.0f) fa = 1.0f;
            s_alpha  = s_alphaFrom + (1.0f - s_alphaFrom) * fa;
        }
    }
}

bool Hidden()      { return s_state == ST_HIDDEN; }
bool HardHidden()  { return s_hardHide; }
bool BlocksInput() { return s_fold > 0.55f; }   // 折过一半就不再吃点击

/** 按键入口: 24=音量+, 25=音量-; 只在按下沿触发 */
void OnVolumeKey(int keyCode, bool down) {
    if (!down) return;
    if (keyCode == 25)      Hide();
    else if (keyCode == 24) Show();
}

/** 在 ImGui::Render() 之后调用: 对顶点/裁剪矩形做折叠与渐显变换 */
void TransformDrawData(ImDrawData* dd) {
    if (dd == nullptr) return;
    const bool foldMode = (s_mode == MD_FOLD);
    const float amt = foldMode ? s_fold : s_scaleK;
    if (amt <= 0.0005f && s_alpha >= 0.9995f) return;
    const float dispY = dd->DisplayPos.y;
    const float H = dd->DisplaySize.y;
    if (H < 2.0f) return;
    const float cx = dd->DisplayPos.x + dd->DisplaySize.x * 0.5f;
    const float sc = 1.0f - s_scaleK;
    const float rot = (s_mode == MD_ICON) ? (s_scaleK * 0.055f) : 0.0f;
    const float cs = cosf(rot), sn = sinf(rot);
    const void* seen[160];
    int seenN = 0;

    for (int n = 0; n < dd->CmdListsCount; ++n) {
        ImDrawList* cl = dd->CmdLists[n];
        if (cl == nullptr) continue;
        ImDrawVert* vtx = cl->VtxBuffer.Data;
        if (vtx != nullptr && cl->VtxBuffer.Size > 0) {
            bool dup = false;
            for (int s2 = 0; s2 < seenN; ++s2) { if (seen[s2] == (const void*)vtx) { dup = true; break; } }
            if (dup) continue;
            if (seenN < 160) seen[seenN++] = (const void*)vtx;
            const int vn = cl->VtxBuffer.Size;
            for (int i = 0; i < vn; ++i) {
                const float y = vtx[i].pos.y - dispY;
                bool keep = false;
                if (foldMode && KeepHit(0, (const void*)cl, i))   keep = true;
                else if (KeepHit(1, (const void*)cl, i))          keep = true;
                if (!keep) {
                    if (foldMode) {
                        float t = y / H;
                        if (t < 0.0f) t = 0.0f;
                        if (t > 1.0f) t = 1.0f;
                        float local = s_fold * 1.35f - t * 0.35f;
                        if (local < 0.0f) local = 0.0f;
                        if (local > 1.0f) local = 1.0f;
                        const float e = local * local * (3.0f - 2.0f * local);
                        vtx[i].pos.y = dispY + y * (1.0f - e);
                        vtx[i].pos.x = cx + (vtx[i].pos.x - cx) * (1.0f - 0.12f * e);
                    } else {
                        const float dx = vtx[i].pos.x - s_iconX;
                        const float dy = vtx[i].pos.y - s_iconY;
                        vtx[i].pos.x = s_iconX + (dx * cs - dy * sn) * sc;
                        vtx[i].pos.y = s_iconY + (dx * sn + dy * cs) * sc;
                    }
                    const unsigned int a0 = (vtx[i].col >> 24) & 0xFFu;
                    unsigned int a1 = (unsigned int)((float)a0 * s_alpha + 0.5f);
                    if (a1 > 255u) a1 = 255u;
                    vtx[i].col = (vtx[i].col & 0x00FFFFFFu) | ((a1 & 0xFFu) << 24);
                }
            }
            for (int c = 0; c < cl->CmdBuffer.Size; ++c) {
                ImDrawCmd& cmd = cl->CmdBuffer[c];
                if (foldMode) {
                    const float ty = cmd.ClipRect.y - dispY;
                    float t = ty / H;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                    float local = s_fold * 1.35f - t * 0.35f;
                    if (local < 0.0f) local = 0.0f;
                    if (local > 1.0f) local = 1.0f;
                    const float e = local * local * (3.0f - 2.0f * local);
                    cmd.ClipRect.y = dispY + ty * (1.0f - e);
                } else {
                    const float x0 = cmd.ClipRect.x, y0 = cmd.ClipRect.y;
                    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
                    const float px[4] = { cmd.ClipRect.x, cmd.ClipRect.z, cmd.ClipRect.x, cmd.ClipRect.z };
                    const float py[4] = { cmd.ClipRect.y, cmd.ClipRect.y, cmd.ClipRect.w, cmd.ClipRect.w };
                    for (int q = 0; q < 4; ++q) {
                        const float dx = px[q] - s_iconX, dy = py[q] - s_iconY;
                        const float rx = s_iconX + (dx * cs - dy * sn) * sc;
                        const float ry = s_iconY + (dx * sn + dy * cs) * sc;
                        if (rx < mnx) mnx = rx;  if (rx > mxx) mxx = rx;
                        if (ry < mny) mny = ry;  if (ry > mxy) mxy = ry;
                    }
                    cmd.ClipRect.x = mnx; cmd.ClipRect.y = mny;
                    cmd.ClipRect.z = mxx; cmd.ClipRect.w = mxy;
                    (void)x0; (void)y0;
                }
            }
        }
    }
}

} // namespace UiAnim