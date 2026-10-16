package com.u3dexample;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * 靶子: 内存里一块 [TARGET_SCORE_01][int score=1] 的结构
 * 作用: 让“内存修改工具”通过 识别进程 -> 扫描标签 -> 写 999, 屏幕上数字真的变成 999
 */
public class RangeActivity extends Activity {

    static { System.loadLibrary("imguidemo"); }

    private native int  nativeGetScore();
    private native void nativeSetScore(int v);
    private native long nativeScoreAddr();
    private native long nativeBaseAddr();
    private native String nativeSelfScan();

    private TextView mScore;
    private TextView mInfo;
    private final Handler mH = new Handler(Looper.getMainLooper());

    private final Runnable mTick = new Runnable() {
        @Override public void run() {
            int v = nativeGetScore();
            mScore.setText(String.valueOf(v));
            // 被外部工具改过就变色提醒
            mScore.setTextColor(v == 1 ? 0xFF2196F3 : 0xFF00C853);
            mInfo.setText("PID  : " + android.os.Process.myPid() + "\n"
                    + "TAG  : TARGET_SCORE_01\n"
                    + "value: " + v + "\n"
                    + "base : 0x" + Long.toHexString(nativeBaseAddr()) + "\n"
                    + "score: 0x" + Long.toHexString(nativeScoreAddr()));
            mH.postDelayed(this, 100L);          // 100ms 刷新
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(0xFF0F1115);
        root.setPadding(40, 40, 40, 40);

        TextView title = new TextView(this);
        title.setText("靶场 · 内存修改练习");
        title.setTextColor(0xFFFFFFFF);
        title.setTextSize(22f);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        mScore = new TextView(this);
        mScore.setTextColor(0xFF2196F3);
        mScore.setTextSize(96f);
        mScore.setGravity(Gravity.CENTER);
        mScore.setPadding(0, 30, 0, 30);
        root.addView(mScore);

        mInfo = new TextView(this);
        mInfo.setTextColor(0xFF9AA0A6);
        mInfo.setTextSize(13f);
        mInfo.setGravity(Gravity.CENTER);
        root.addView(mInfo);

        Button add = new Button(this);
        add.setText("手动 +1");
        add.setOnClickListener(v -> nativeSetScore(nativeGetScore() + 1));
        root.addView(add);

        Button reset = new Button(this);
        reset.setText("重置为 1");
        reset.setOnClickListener(v -> nativeSetScore(1));
        root.addView(reset);

        Button self = new Button(this);
        self.setText("自检(扫自己的内存)");
        root.addView(self);

        final TextView mSelf = new TextView(this);
        mSelf.setTextColor(0xFFCCCCCC);
        mSelf.setTextSize(12f);
        mSelf.setGravity(Gravity.CENTER);
        mSelf.setPadding(20, 20, 20, 20);
        root.addView(mSelf);
        self.setOnClickListener(v -> mSelf.setText(nativeSelfScan()));

        setContentView(root);
    }

    @Override protected void onResume() { super.onResume(); mH.post(mTick); }
    @Override protected void onPause()  { mH.removeCallbacks(mTick); super.onPause(); }
}