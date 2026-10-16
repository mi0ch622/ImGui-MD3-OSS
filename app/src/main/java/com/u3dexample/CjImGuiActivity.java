package com.u3dexample;

import android.content.Intent;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;

import java.util.Locale;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.appcompat.app.AppCompatActivity;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * ImGui 中文渲染 Demo (GLSurfaceView + OpenGL ES 2):
 * - EGL context 由 GLSurfaceView 框架管理
 * - 触摸: GLSurfaceView.setOnTouchListener (相对 View 坐标)
 * - 软键盘: C++ SetPlatformImeDataFn -> showIme()/hideIme() -> InputMethodManager
 *   showIme/hideIme 用 runOnUiThread 包裹, 因为回调来自 GL 线程
 * - 文本输入: ImGuiGLSurfaceView.onCreateInputConnection 拦截 IME commitText
 *   -> nativeOnChar (逐字符) 或 nativeOnCommitText (整串)
 * - 按键: onKeyDown/onKeyUp -> nativeOnKey
 */
public class CjImGuiActivity extends AppCompatActivity {

    static {
        System.loadLibrary("imguidemo");
    }

    private ImGuiGLSurfaceView mGLView;
    private InputMethodManager mIMM;

    // ===== JNI =====
    private native void nativeInit(CjImGuiActivity activity, android.view.Surface surface);
    private native void nativeResize(int width, int height);
    private native void nativeRenderFrame();
    private native void nativeShutdown();
    private native void nativeOnTouch(int action, float x, float y);
    private native void nativeOnKey(int keyCode, boolean down);
    private native void nativeOnChar(int unicodeChar);
    private native void nativeOnCommitText(String text);
    private native void nativeOnDeleteChar();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mIMM = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        NetInfoBridge.attach(this);
        WorkspaceManager.attach(this);
        McpManager.attach(this);
        handleShareIntent(getIntent());

        mGLView = new ImGuiGLSurfaceView(this);
        mGLView.setEGLContextClientVersion(2);
        mGLView.setEGLConfigChooser(8, 8, 8, 8, 0, 0);
        mGLView.setRenderer(new Renderer());
        mGLView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setContentView(mGLView);

        // IME 文本回调: 中文输入法 commitText -> 逐字符 nativeOnChar
        mGLView.setImeCallback(new ImGuiGLSurfaceView.ImeCallback() {
            @Override
            public void onCommitText(String text) {
                for (int i = 0; i < text.length(); i++) {
                    nativeOnChar(text.charAt(i));
                }
            }

            @Override
            public void onDeleteChar() {
                nativeOnDeleteChar();
            }
        });

        // 触摸: 相对 View 本地坐标系
        mGLView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_DOWN
                        || action == MotionEvent.ACTION_MOVE
                        || action == MotionEvent.ACTION_UP) {
                    nativeOnTouch(action, event.getX(), event.getY());
                    return true;
                }
                return false;
            }
        });
    }

    // ===== 软键盘控制 (由 C++ SetPlatformImeDataFn 在 GL 线程回调) =====
    // 必须切到 UI 线程操作 View (requestFocus / showSoftInput)

    public void showIme() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mGLView.requestFocus();
                mIMM.showSoftInput(mGLView, InputMethodManager.SHOW_FORCED);
            }
        });
    }

    public void hideIme() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mIMM.hideSoftInputFromWindow(mGLView.getWindowToken(), 0);
            }
        });
    }

    /** 由 C++ 侧 Markdown 链接点击回调，用系统浏览器打开。 */
    public void openUrl(final String url) {
        if (url == null || url.length() == 0) return;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent i = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url));
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(i);
                } catch (Throwable ignored) {
                }
            }
        });
    }

    /** 由 C++ 侧调用：写入系统剪贴板（避免使用系统长按选择浮层，那会和 GLSurfaceView 冲突卡死）。 */
    public void setClipboard(final String text) {
        if (text == null) return;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    android.content.ClipboardManager cm = (android.content.ClipboardManager)
                            getSystemService(CLIPBOARD_SERVICE);
                    if (cm != null) {
                        cm.setPrimaryClip(android.content.ClipData.newPlainText("Avates", text));
                    }
                } catch (Throwable ignored) {
                }
            }
        });
    }

    /** 由 C++ 侧调用：读取系统剪贴板文本。 */
    public String getClipboard() {
        try {
            android.content.ClipboardManager cm = (android.content.ClipboardManager)
                    getSystemService(CLIPBOARD_SERVICE);
            if (cm != null && cm.hasPrimaryClip() && cm.getPrimaryClip() != null
                    && cm.getPrimaryClip().getItemCount() > 0) {
                android.content.ClipData.Item item = cm.getPrimaryClip().getItemAt(0);
                if (item != null && item.getText() != null) {
                    return item.getText().toString();
                }
            }
        } catch (Throwable ignored) {
        }
        return "";
    }

    /**
     */
    private void handleShareIntent(Intent intent) {
        if (intent == null) return;
        try {
            String action = intent.getAction();
            String type = intent.getType();
            if (Intent.ACTION_SEND.equals(action)) {
                if (type != null && type.startsWith("text/")) {
                    String txt = intent.getStringExtra(Intent.EXTRA_TEXT);
                    if (txt != null && txt.length() > 0) WorkspaceManager.importSharedText(txt);
                } else {
                    android.net.Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                    if (uri != null) {
                        WorkspaceManager.importSharedUri(uri, null, type);
                    }
                }
            } else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
                java.util.ArrayList<android.net.Uri> list =
                        intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (list != null) {
                    for (int i = 0; i < list.size(); i++) {
                        WorkspaceManager.importSharedUri(list.get(i), null, type);
                    }
                }
            } else if (Intent.ACTION_VIEW.equals(action)) {
                String txt = intent.getStringExtra(Intent.EXTRA_TEXT);
                if (txt != null && txt.length() > 0) WorkspaceManager.importSharedText(txt);
            }
        } catch (Throwable e) {
            WorkspaceManager.appendInbox("处理分享内容失败：" + e);
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleShareIntent(intent);
    }

    /** 打开系统目录选择器（SAF），选完把 tree URI 尽量还原成真实路径 */
    public void pickWorkspaceDir() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                    i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    startActivityForResult(i, REQ_TREE);
                } catch (Throwable e) {
                    WorkspaceManager.appendInbox("无法打开目录选择器：" + e);
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_TREE || resultCode != RESULT_OK || data == null) return;
        try {
            android.net.Uri uri = data.getData();
            if (uri == null) return;
            try {
                getContentResolver().takePersistableUriPermission(uri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            } catch (Throwable ignored) { }
            String path = treeUriToPath(uri);
            if (path != null && path.length() > 0) {
                WorkspaceManager.setRootPath(path);
                WorkspaceManager.appendInbox("工作区已切换为：" + path);
            } else {
                WorkspaceManager.appendInbox("该目录无法还原为真实路径，请手动输入路径");
            }
        } catch (Throwable e) {
            WorkspaceManager.appendInbox("设置工作区失败：" + e);
        }
    }

    /** tree URI -> 真实路径（对 primary / sdcard 这类常见 provider 有效） */
    private static String treeUriToPath(android.net.Uri uri) {
        try {
            String id = android.provider.DocumentsContract.getTreeDocumentId(uri);
            if (id == null) return null;
            String[] parts = id.split(":", 2);
            String vol = parts[0];
            String rel = parts.length > 1 ? parts[1] : "";
            String base;
            if ("primary".equalsIgnoreCase(vol)) {
                base = android.os.Environment.getExternalStorageDirectory().getAbsolutePath();
            } else if (vol.length() == 0) {
                base = "/";
            } else {
                base = "/storage/" + vol;
            }
            return rel.length() == 0 ? base : (base + "/" + rel);
        } catch (Throwable e) {
            return null;
        }
    }

    private static final int REQ_TREE = 0x5101;

    // ===== 物理按键转发 (软键盘的字符走 InputConnection, 不走这里) =====

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // 音量键改作 UI 动效控制（音量- 折叠隐藏 / 音量+ 渐显显示），
        // 事件在这里吃掉，系统音量因此不再变化。
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            nativeOnKey(keyCode, true);
            return true;
        }
        nativeOnKey(keyCode, true);
        // 物理键盘 / ADB 注入的按键事件: 把可打印字符直接送进 ImGui 输入缓冲。
        // 软键盘走 InputConnection.commitText, 不经过 onKeyDown, 因此不会重复输入。
        if (keyCode != KeyEvent.KEYCODE_ENTER
                && keyCode != KeyEvent.KEYCODE_DEL
                && keyCode != KeyEvent.KEYCODE_TAB) {
            int unicode = event.getUnicodeChar();
            if (unicode >= 0x20 && unicode != '\n' && unicode != '\r') {
                nativeOnChar(unicode);
            }
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            nativeOnKey(keyCode, false);
            return true;
        }
        nativeOnKey(keyCode, false);
        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mGLView.onResume();
    }

    @Override
    protected void onPause() {
        mGLView.onPause();
        hideIme();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (mTts != null) { mTts.stop(); mTts.shutdown(); mTts = null; mTtsReady = false; }
        nativeShutdown();
        super.onDestroy();
    }

    private class Renderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            SurfaceHolder holder = mGLView.getHolder();
            nativeInit(CjImGuiActivity.this, holder.getSurface());
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            nativeResize(width, height);
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            nativeRenderFrame();
        }
    }

    // ================= 立绘语音 (TTS) =================
    private TextToSpeech mTts;
    private volatile boolean mTtsReady = false;
    private volatile float mTtsPitch = 1.55f;   // 萝莉音: 音调偏高
    private volatile float mTtsRate  = 1.08f;   // 稍快一点更活泼

    public void ttsInit() {
        if (mTts != null) return;
        runOnUiThread(() -> ttsInitOnUi());
    }

    private void ttsInitOnUi() {
        if (mTts != null) return;
        mTts = new TextToSpeech(getApplicationContext(), status -> {
            if (status == TextToSpeech.SUCCESS && mTts != null) {
                int r = mTts.setLanguage(Locale.CHINA);
                if (r == TextToSpeech.LANG_MISSING_DATA || r == TextToSpeech.LANG_NOT_SUPPORTED) {
                    mTts.setLanguage(Locale.getDefault());
                }
                mTts.setPitch(mTtsPitch);
                mTts.setSpeechRate(mTtsRate);
                mTtsReady = true;
            } else {
                mTtsReady = false;
            }
        });
    }

    public void ttsSay(String text) {
        if (text == null || text.isEmpty()) return;
        final String t = text;
        runOnUiThread(() -> {
            if (mTts == null) ttsInitOnUi();
            if (mTts == null || !mTtsReady) return;
            mTts.speak(t, TextToSpeech.QUEUE_ADD, null, "u3d" + System.nanoTime());
        });
    }

    public void ttsStop() { if (mTts != null) mTts.stop(); }

    public void ttsSetPitch(final float p) {
        mTtsPitch = p;
        runOnUiThread(() -> { if (mTts != null) mTts.setPitch(p); });
    }

    public void ttsSetRate(final float r) {
        mTtsRate = r;
        runOnUiThread(() -> { if (mTts != null) mTts.setSpeechRate(r); });
    }

    public boolean ttsReady() { return mTtsReady; }

}
