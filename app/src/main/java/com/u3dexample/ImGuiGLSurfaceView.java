package com.u3dexample;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * 自定义 GLSurfaceView:
 * - 重写 onCreateInputConnection, 用 BaseInputConnection 自建 (不依赖 super 返回 null)
 * - 拦截 IME commitText (中文输入法走此通道)
 * - 设 IME_FLAG_NO_EXTRACT_UI | IME_FLAG_NO_FULLSCREEN 避免遮蔽半屏
 */
public class ImGuiGLSurfaceView extends GLSurfaceView {

    public interface ImeCallback {
        void onCommitText(String text);
        void onDeleteChar();
    }

    private ImeCallback mImeCallback;
    private CharSequence mComposing = "";
    private boolean      mCommittedInComposing = false;

    public ImGuiGLSurfaceView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public void setImeCallback(ImeCallback callback) {
        mImeCallback = callback;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // 关键: 避免系统 IME 进入全屏编辑模式 (遮蔽半屏)
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT;

        // 不调 super.onCreateInputConnection (GLSurfaceView 默认返回 null)
        // 用 BaseInputConnection 自建, targetView = this
        return new BaseInputConnection(this, false) {
            @Override
            public boolean commitText(CharSequence text, int newCursorPosition) {
                mCommittedInComposing = true;
                if (mImeCallback != null && text != null && text.length() > 0) {
                    mImeCallback.onCommitText(text.toString());
                }
                return true;
            }

            @Override
            public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                if (mImeCallback != null && beforeLength > 0) {
                    for (int i = 0; i < beforeLength; i++) {
                        mImeCallback.onDeleteChar();
                    }
                }
                return true;
            }

            @Override
            public boolean sendKeyEvent(android.view.KeyEvent event) {
                if (mImeCallback != null && event.getAction() == android.view.KeyEvent.ACTION_UP) {
                    int code = event.getKeyCode();
                    if (code == android.view.KeyEvent.KEYCODE_DEL) {
                        mImeCallback.onDeleteChar();
                    } else {
                        int unicode = event.getUnicodeChar();
                        if (unicode != 0) {
                            mImeCallback.onCommitText(String.valueOf((char) unicode));
                        }
                    }
                }
                return true;
            }

            @Override
            public boolean finishComposingText() {
                // 语音输入法 / 部分输入法只走 setComposingText + finishComposingText，
                // 全程不调 commitText。这里兜底补交，否则语音识别的字会被整个吞掉。
                if (!mCommittedInComposing && mImeCallback != null
                        && mComposing != null && mComposing.length() > 0) {
                    mImeCallback.onCommitText(mComposing.toString());
                }
                mComposing = "";
                mCommittedInComposing = false;
                return true;
            }

            @Override
            public boolean setComposingText(CharSequence text, int newCursorPosition) {
                // 拼音输入法的实时预览（候选词未确认）不能直接转发，否则会把拼音字母写进去；
                // 先记录下来，等 finishComposingText 时按情况兜底提交。
                mComposing = text;
                return true;
            }
        };
    }
}
