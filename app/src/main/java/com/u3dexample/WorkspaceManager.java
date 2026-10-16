package com.u3dexample;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Environment;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 *
 * 安全约束：所有相对路径解析后必须仍落在工作区目录内（防 ../ 逃逸）。
 * 配置以 JSON 形式暴露给 C++ 侧设置界面（getConfigJson / setConfigJson）。
 */
public final class WorkspaceManager {

    private static final String PREF   = "avates_cfg";
    private static final String KEY_WS = "workspace";

    private static Context sCtx;
    private static volatile String sInboxNote = "";

    private WorkspaceManager() { }

    public static void attach(Context c) {
        if (c == null) return;
        sCtx = c.getApplicationContext();
        try {
            File d = new File(rootPath());
            if (!d.exists()) d.mkdirs();
        } catch (Throwable ignored) { }
    }

    // =================================================================
    //  配置
    // =================================================================
    public static SharedPreferences prefs() {
        return sCtx.getSharedPreferences(PREF, Context.MODE_PRIVATE);
    }

    public static String cfgGet(String k, String def) {
        try { return prefs().getString(k, def); } catch (Throwable e) { return def; }
    }

    public static void cfgPut(String k, String v) {
        try { prefs().edit().putString(k, v).apply(); } catch (Throwable ignored) { }
    }

    /** 默认工作区：App 私有目录，免任何权限就能读写 */
    public static String defaultPath() {
        try {
            File ext = sCtx.getExternalFilesDir(null);
            if (ext != null) return new File(ext, "workspace").getAbsolutePath();
        } catch (Throwable ignored) { }
        return "/data/local/tmp/avates_workspace";
    }

    public static String rootPath() {
        String p = cfgGet(KEY_WS, "");
        if (p == null || p.trim().length() == 0) p = defaultPath();
        return p.trim();
    }

    public static void setRootPath(String p) {
        cfgPut(KEY_WS, p == null ? "" : p.trim());
    }

    public static String statusText() {
        String p = rootPath();
        File f = new File(p);
        boolean exists = f.exists();
        if (!exists) { try { exists = f.mkdirs(); } catch (Throwable ignored) { } }
        boolean w = exists && f.canWrite();
        boolean r = exists && f.canRead();
        return (w && r) ? "可读写" : (exists ? "不可写" : "目录不存在");
    }

    // =================================================================
    //  路径安全解析
    // =================================================================
    private static File resolve(String rel) throws Exception {
        File root = new File(rootPath());
        if (!root.exists()) root.mkdirs();
        File target;
        if (rel == null || rel.trim().length() == 0 || rel.equals(".") || rel.equals("/")) {
            target = root;
        } else {
            String r = rel.trim().replace('\\', '/');
            File f = new File(r);
            target = f.isAbsolute() ? f : new File(root, r);
        }
        String rootCanon = root.getCanonicalPath();
        String tCanon = target.getCanonicalPath();
        if (!tCanon.equals(rootCanon) && !tCanon.startsWith(rootCanon + File.separator)) {
            throw new Exception("路径越界：只允许访问工作区内的文件（" + rootCanon + "）");
        }
        return target;
    }

    private static String relOf(File f) {
        try {
            String root = new File(rootPath()).getCanonicalPath();
            String c = f.getCanonicalPath();
            if (c.equals(root)) return ".";
            if (c.startsWith(root + File.separator)) return c.substring(root.length() + 1);
            return c;
        } catch (Throwable e) { return f.getName(); }
    }

    // =================================================================
    // =================================================================
    public static String listDir(String rel) {
        try {
            File d = resolve(rel);
            if (!d.exists()) return "目录不存在：" + relOf(d);
            if (!d.isDirectory()) return "不是目录：" + relOf(d);
            File[] kids = d.listFiles();
            StringBuilder sb = new StringBuilder();
            sb.append("工作区：").append(rootPath()).append("\n");
            sb.append("目录：").append(relOf(d)).append("\n");
            if (kids == null || kids.length == 0) { sb.append("（空）\n"); return sb.toString(); }
            int n = 0;
            for (File k : kids) {
                if (n++ >= 200) { sb.append("...（已截断）\n"); break; }
                sb.append(k.isDirectory() ? "[D] " : "[F] ")
                  .append(k.getName())
                  .append(k.isDirectory() ? "/" : ("  " + k.length() + "B"))
                  .append("\n");
            }
            return sb.toString();
        } catch (Throwable e) {
            return "列目录失败：" + e.getMessage();
        }
    }

    public static String readFile(String rel, int maxBytes) {
        try {
            File f = resolve(rel);
            if (!f.exists()) return "文件不存在：" + relOf(f);
            if (f.isDirectory()) return "这是目录，不是文件：" + relOf(f);
            long len = f.length();
            int cap = maxBytes <= 0 ? 65536 : Math.min(maxBytes, 262144);
            byte[] buf = new byte[(int) Math.min(len, cap)];
            FileInputStream in = new FileInputStream(f);
            int off = 0;
            try {
                while (off < buf.length) {
                    int r = in.read(buf, off, buf.length - off);
                    if (r <= 0) break;
                    off += r;
                }
            } finally { try { in.close(); } catch (Throwable ignored) { } }
            String body = new String(buf, 0, off, "UTF-8");
            String head = "文件：" + relOf(f) + "  (" + len + " B" + (len > off ? "，仅显示前 " + off + " B" : "") + ")\n---\n";
            return head + body;
        } catch (Throwable e) {
            return "读文件失败：" + e.getMessage();
        }
    }

    public static String writeFile(String rel, String content) {
        try {
            File f = resolve(rel);
            File p = f.getParentFile();
            if (p != null && !p.exists()) p.mkdirs();
            FileOutputStream out = new FileOutputStream(f, false);
            try {
                out.write(content == null ? new byte[0] : content.getBytes("UTF-8"));
                out.flush();
            } finally { try { out.close(); } catch (Throwable ignored) { } }
            return "已写入 " + relOf(f) + "（" + f.length() + " B）";
        } catch (Throwable e) {
            return "写文件失败：" + e.getMessage();
        }
    }

    public static String makeDir(String rel) {
        try {
            File f = resolve(rel);
            if (f.exists()) return "目录已存在：" + relOf(f);
            return f.mkdirs() ? ("已创建目录 " + relOf(f)) : ("创建目录失败：" + relOf(f));
        } catch (Throwable e) {
            return "创建目录失败：" + e.getMessage();
        }
    }

    // =================================================================
    //  分享进 App：把外部内容落到工作区
    // =================================================================
    public static void appendInbox(String line) {
        if (line == null || line.length() == 0) return;
        sInboxNote = sInboxNote.length() == 0 ? line : (sInboxNote + "\n" + line);
    }

    public static String takeInboxNote() {
        String s = sInboxNote;
        sInboxNote = "";
        return s;
    }

    public static String importSharedText(String text) {
        try {
            String name = "shared_" + System.currentTimeMillis() + ".txt";
            File f = resolve(name);
            FileOutputStream out = new FileOutputStream(f, false);
            try { out.write((text == null ? "" : text).getBytes("UTF-8")); } finally {
                try { out.close(); } catch (Throwable ignored) { }
            }
            appendInbox("已收到分享文本 → " + name);
            return name;
        } catch (Throwable e) {
            appendInbox("分享文本保存失败：" + e.getMessage());
            return null;
        }
    }

    public static String importSharedUri(Uri uri, String display, String mime) {
        try {
            if (sCtx == null || uri == null) return null;
            String name = display;
            if (name == null || name.trim().length() == 0) {
                name = "shared_" + System.currentTimeMillis();
                String ext = android.webkit.MimeTypeMap.getSingleton().getExtensionFromMimeType(mime);
                if (ext != null) name += "." + ext;
            }
            name = name.replace('/', '_').replace('\\', '_');
            File dst = resolve(name);
            InputStream in = sCtx.getContentResolver().openInputStream(uri);
            if (in == null) { appendInbox("分享文件读取失败"); return null; }
            FileOutputStream out = new FileOutputStream(dst, false);
            try {
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                out.flush();
            } finally {
                try { out.close(); } catch (Throwable ignored) { }
                try { in.close(); } catch (Throwable ignored) { }
            }
            appendInbox("已收到分享文件 → " + name + "（" + dst.length() + " B）");
            return name;
        } catch (Throwable e) {
            appendInbox("分享文件保存失败：" + e.getMessage());
            return null;
        }
    }

    // =================================================================
    //  给 C++ 设置界面用的 JSON 配置
    // =================================================================
    public static String getConfigJson() {
        String ws = rootPath();
        String servers = McpManager.serversJson();
        return "{\"workspace\":\"" + esc(ws) + "\",\"workspaceStatus\":\"" + esc(statusText())
             + "\",\"defaultWorkspace\":\"" + esc(defaultPath())
             + "\",\"servers\":" + servers + "}";
    }

    public static void setConfigJson(String json) {
        try {
            org.json.JSONObject o = new org.json.JSONObject(json);
            if (o.has("workspace")) setRootPath(o.optString("workspace", defaultPath()));
            if (o.has("servers")) McpManager.saveServers(o.optJSONArray("servers"));
        } catch (Throwable ignored) { }
    }

    public static String esc(String s) {
        if (s == null) return "";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c == '\n') b.append("\\n");
            else if (c == '\r') b.append("\\r");
            else if (c == '\t') b.append("\\t");
            else b.append(c);
        }
        return b.toString();
    }

    public static List<File> listWorkspaceFiles() {
        List<File> out = new ArrayList<File>();
        try {
            File[] k = new File(rootPath()).listFiles();
            if (k != null) for (File f : k) out.add(f);
        } catch (Throwable ignored) { }
        return out;
    }
}