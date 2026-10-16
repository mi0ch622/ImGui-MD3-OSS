package com.u3dexample;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.MotionEvent;

import java.io.File;
import java.io.FileInputStream;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * 目的: 验证 Java 层能不能读到进程与 PID -> 点“判断进程”就显示 PID
 */
public class ProcTestActivity extends Activity {

    static { System.loadLibrary("imguidemo"); }

    private native void nativePTInit(Object surface);
    private native void nativePTResize(int w, int h);
    private native void nativePTFrame();
    private native void nativePTTouch(int action, float x, float y);

    private GLSurfaceView mView;

    public static String probeProcess(String key) {
        StringBuilder sb = new StringBuilder();
        int total = 0, hit = 0;
        String k = (key == null) ? "" : key.toLowerCase();
        try {
            File[] fs = new File("/proc").listFiles();
            if (fs != null) {
                for (File f : fs) {
                    String n = f.getName();
                    if (n.isEmpty() || n.charAt(0) < '0' || n.charAt(0) > '9') continue;
                    String name = "";
                    try (FileInputStream in = new FileInputStream("/proc/" + n + "/cmdline")) {
                        byte[] buf = new byte[512];
                        int r = in.read(buf);
                        if (r > 0) {
                            StringBuilder nm = new StringBuilder();
                            for (int i = 0; i < r; i++) {
                                byte c = buf[i];
                                if (c == 0) break;
                                nm.append((char) (c & 0xFF));
                            }
                            name = nm.toString();
                        }
                    } catch (Throwable ignore) { }
                    if (name.isEmpty()) name = "(no-cmdline)";
                    total++;
                    if (k.isEmpty() || name.toLowerCase().contains(k)) {
                        hit++;
                        sb.append("PID ").append(n).append("  ").append(name).append('\n');
                    }
                }
            }
        } catch (Throwable e) {
            return "ERR " + e;
        }
        return "self_pid=" + android.os.Process.myPid()
                + "  total=" + total + "  hit=" + hit + "\n--------\n" + sb;
    }

    public static String sh(String cmd) {
        StringBuilder sb = new StringBuilder();
        try {
            Process p = new ProcessBuilder("sh", "-c", cmd).redirectErrorStream(true).start();
            java.io.BufferedReader r =
                    new java.io.BufferedReader(new java.io.InputStreamReader(p.getInputStream()));
            String l;
            while ((l = r.readLine()) != null) sb.append(l).append('\n');
            r.close();
            sb.append("[exit=").append(p.waitFor()).append(']');
        } catch (Throwable e) {
            sb.append("ERR ").append(e);
        }
        return sb.toString();
    }

    /**
     * 不做任何 root 前置判断; 直连失败会自动退回 su 通道 (框架 / root 环境)。
     * ● 包名 -> 枚举该包【全部】进程, 逐个试 (多进程 App 也能命中)
     * ● 只扫可写(rw)段, 找 TARGET_SCORE_01, 地址 +24 = score
     * ● 写完立刻从目标进程读回校验, 结果里打出“读回=?”
     */
    public static String patchTarget(String pkgOrPid, String valTxt) {
        int val = 999;
        try { val = Integer.parseInt(valTxt.trim()); } catch (Throwable ignore) { }
        String key = (pkgOrPid == null) ? "" : pkgOrPid.trim();

        java.util.List<Integer> pids = new java.util.ArrayList<>();
        if (key.matches("\\d+")) {
            pids.add(Integer.parseInt(key));
        } else {
            pids.addAll(pidsByCmdline(key));
            int p1 = pidByPidof(key);
            if (p1 > 0 && !pids.contains(p1)) pids.add(p1);
        }
        if (pids.isEmpty()) {
            return "❌ 识别不到进程: " + key
                    + "\n(看不到别的进程时, 直接在输入框里填 PID 数字即可)";
        }

        StringBuilder rep = new StringBuilder();
        rep.append("目标 = ").append(key).append("   候选PID = ").append(pids).append('\n');
        int self = android.os.Process.myPid();
        for (int pid : pids) {
            if (pid <= 0 || pid == self) continue;
            String r = patchPid(pid, val);
            rep.append("[PID ").append(pid).append("] ").append(r).append('\n');
            if (r.startsWith("✅")) return rep.toString();
        }
        return rep.toString();
    }

    /** 单个 PID: 扫可写段 -> 找标签 -> 写值 -> 读回校验 */
    private static String patchPid(int pid, int val) {
        try {
            // ---- 打开 maps / mem ----
            java.io.BufferedReader maps = new java.io.BufferedReader(
                    new java.io.FileReader("/proc/" + pid + "/maps"));
            java.io.RandomAccessFile mem = new java.io.RandomAccessFile(
                    "/proc/" + pid + "/mem", "rw");
            byte[] tag = "TARGET_SCORE_01".getBytes("UTF-8");

            long total = 0;
            String line;
            while ((line = maps.readLine()) != null) {
                String[] p = line.split("\\s+");
                if (p.length < 2) continue;
                String[] rng = p[0].split("-");
                if (rng.length != 2) continue;
                if (!p[1].startsWith("rw")) continue;
                long a, b;
                try {
                    a = Long.parseLong(rng[0], 16);
                    b = Long.parseLong(rng[1], 16);
                } catch (Throwable ignore) { continue; }
                long len = b - a;
                if (len <= 0 || len > 512L * 1024 * 1024) continue;

                // 分块扫描(1MB/块, 块间重叠标签长度, 防止标签跨块)
                final int CH = 1 << 20;
                byte[] buf = new byte[CH + 64];
                for (long base = a; base < b; ) {
                    int want = (int) Math.min((long) buf.length, b - base);
                    int got;
                    try {
                        mem.seek(base);
                        got = mem.read(buf, 0, want);
                    } catch (Throwable e) { break; }
                    if (got <= 0) break;
                    int off = indexOf(buf, got, tag, 0);
                    while (off >= 0) {
                        //    内存里常常还有同名字符串副本(只读区/堆/JIT), 写到副本后面就会
                        //    出现“显示成功、读回也对, 但目标界面毫无变化” —— 必须靠这个特征排除。
                        boolean isReal = (off + 32 <= got)
                                && (buf[off + 28] & 0xFF) == 0x34
                                && (buf[off + 29] & 0xFF) == 0x12;
                        if (!isReal) {
                            off = indexOf(buf, got, tag, off + 1);
                            continue;
                        }
                        long tagAddr = base + off;
                        long scoreAddr = tagAddr + 24;
                        try {
                            mem.seek(scoreAddr);
                            mem.write(new byte[]{
                                    (byte) (val & 0xFF),
                                    (byte) ((val >> 8) & 0xFF),
                                    (byte) ((val >> 16) & 0xFF),
                                    (byte) ((val >> 24) & 0xFF) });
                            byte[] back = new byte[4];
                            mem.seek(scoreAddr);
                            int n = mem.read(back, 0, 4);
                            long readBack = (n == 4)
                                    ? ((back[0] & 0xFFL) | ((back[1] & 0xFFL) << 8)
                                       | ((back[2] & 0xFFL) << 16) | ((back[3] & 0xFFL) << 24))
                                    : -1L;
                            mem.close(); maps.close();
                            return "✅ 写入+读回校验\n标签地址 = 0x" + Long.toHexString(tagAddr)
                                    + "\nscore地址 = 0x" + Long.toHexString(scoreAddr)
                                    + "\n写入 = " + val + "   读回 = " + readBack
                                    + (readBack == val ? "  (一致 ✓)" : "  ✗ 不一致(写没落到这块内存)")
                                    + "\n(已扫 " + (total / 1024) + " KB 可写段)";
                        } catch (Throwable e) {
                            mem.close(); maps.close();
                            return "找到标签 @" + Long.toHexString(tagAddr)
                                    + " 但写入失败: " + e;
                        }
                    }
                    if (got <= tag.length) break;
                    base += got - tag.length;
                    total += got;
                }
            }
            mem.close(); maps.close();
            return "✗ 扫了 " + (total / 1024) + " KB 可写段, 没找到 TARGET_SCORE_01 (或读 mem 被拒)\n"
                    + "   -> 下一步会自动试 su 通道";
        } catch (Throwable e) {
            return suPatch(pid, val, e);
        }
    }

    /**
     * su -c 通道: 环境里有 root 时用 (把同一套“扫可写段+写标签后 24 字节”交给 su 跑)
     */
    private static String suPatch(int pid, int val, Throwable cause) {
        try {
            if (!sh("su -c id 2>&1").contains("uid=0")) {
                return "✗ 直连失败(" + cause + ") , 且没有 su";
            }
            StringBuilder oct = new StringBuilder();
            for (int i = 0; i < 4; i++) oct.append(String.format("\\%03o", (val >> (8 * i)) & 0xFF));

            final String script =
                    "P=$1; T=TARGET_SCORE_01\n" +
                    "printf '" + oct + "' > /data/local/tmp/v.bin\n" +
                    "for r in $(grep ' rw' /proc/$P/maps | cut -d' ' -f1); do\n" +
                    "  s=${r%-*}; e=${r#*-}; ss=$((0x$s)); L=$((0x$e-0x$s))\n" +
                    "  [ $L -gt 67108864 ] && continue\n" +
                    "  dd if=/proc/$P/mem bs=65536 iflag=skip_bytes,count_bytes skip=$ss count=$L " +
                            "of=/data/local/tmp/r.bin 2>/dev/null || continue\n" +
                    "  o=$(grep -abo \"$T\" /data/local/tmp/r.bin | head -1 | cut -d: -f1)\n" +
                    "  [ -n \"$o\" ] && { A=$((ss+o+24)); " +
                            "dd if=/data/local/tmp/v.bin of=/proc/$P/mem bs=1 seek=$A conv=notrunc 2>/dev/null; " +
                            "echo SU_OK_AT=0x$(printf %x $A); exit 0; }\n" +
                    "done\n" +
                    "echo SU_NOT_FOUND\n";
            java.io.FileOutputStream fos = new java.io.FileOutputStream("/data/local/tmp/mp_tool.sh");
            fos.write(script.getBytes("UTF-8"));
            fos.close();
            String out = sh("su -c 'sh /data/local/tmp/mp_tool.sh " + pid + " " + val + "'");
            return out.contains("SU_OK_AT")
                    ? "✅ (su 通道) " + out.trim()
                    : "✗ su 通道也未命中: " + out.trim();
        } catch (Throwable e2) {
            return "✗ 直连失败(" + cause + ") / su 失败(" + e2 + ")";
        }
    }

    /** 扫 /proc/*&#47;cmdline 把该包的【所有】PID 都收上来 (多进程 App 也能命中) */
    private static java.util.List<Integer> pidsByCmdline(String pkg) {
        java.util.List<Integer> out = new java.util.ArrayList<>();
        if (pkg == null || pkg.isEmpty()) return out;
        try {
            java.io.File[] ds = new java.io.File("/proc").listFiles();
            if (ds == null) return out;
            for (java.io.File d : ds) {
                if (!d.getName().matches("\\d+")) continue;
                try {
                    byte[] b = new byte[256];
                    java.io.FileInputStream in = new java.io.FileInputStream(d.getPath() + "/cmdline");
                    int n = in.read(b);
                    in.close();
                    if (n <= 0) continue;
                    String s = new String(b, 0, n).replace('\0', ' ').trim();
                    if (s.equals(pkg) || s.startsWith(pkg)) out.add(Integer.parseInt(d.getName()));
                } catch (Throwable ignore) { }
            }
        } catch (Throwable ignore) { }
        return out;
    }

    private static int pidByPidof(String pkg) {
        try {
            String out = sh("pidof " + pkg).trim();
            for (String t : out.split("[^0-9]+")) {
                if (!t.isEmpty()) return Integer.parseInt(t);
            }
        } catch (Throwable ignore) { }
        return -1;
    }

    private static int indexOf(byte[] hay, int hayLen, byte[] needle) {
        return indexOf(hay, hayLen, needle, 0);
    }

    private static int indexOf(byte[] hay, int hayLen, byte[] needle, int from) {
        outer:
        for (int i = Math.max(0, from); i + needle.length <= hayLen; i++) {
            for (int j = 0; j < needle.length; j++) {
                if (hay[i + j] != needle[j]) continue outer;
            }
            return i;
        }
        return -1;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mView = new GLSurfaceView(this);
        mView.setEGLContextClientVersion(2);
        mView.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
        mView.setRenderer(new GLSurfaceView.Renderer() {
            @Override public void onSurfaceCreated(GL10 gl, EGLConfig cfg) {
                nativePTInit(mView.getHolder().getSurface());
            }
            @Override public void onSurfaceChanged(GL10 gl, int w, int h) {
                nativePTResize(w, h);
            }
            @Override public void onDrawFrame(GL10 gl) {
                nativePTFrame();
            }
        });
        mView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        //   (之前挂在 Activity.onTouchEvent 上, 坐标含状态栏偏移 -> 点击偏)
        mView.setFocusable(true);
        mView.setFocusableInTouchMode(true);
        mView.setOnTouchListener(new android.view.View.OnTouchListener() {
            @Override
            public boolean onTouch(android.view.View v, MotionEvent event) {
                int a = event.getActionMasked();
                int act = (a == MotionEvent.ACTION_DOWN) ? 0
                        : (a == MotionEvent.ACTION_UP) ? 1
                        : (a == MotionEvent.ACTION_MOVE) ? 2 : -1;
                if (act >= 0) nativePTTouch(act, event.getX(), event.getY());
                return true;
            }
        });
        setContentView(mView);
    }

    @Override protected void onResume() { super.onResume(); if (mView != null) mView.onResume(); }
    @Override protected void onPause()  { if (mView != null) mView.onPause(); super.onPause(); }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int a = e.getActionMasked();
        int act = (a == MotionEvent.ACTION_DOWN) ? 0
                : (a == MotionEvent.ACTION_UP) ? 1
                : (a == MotionEvent.ACTION_MOVE) ? 2 : -1;
        if (act >= 0) nativePTTouch(act, e.getX(), e.getY());
        return true;
    }
}
