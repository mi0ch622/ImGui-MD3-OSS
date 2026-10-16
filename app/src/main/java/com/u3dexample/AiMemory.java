package com.u3dexample;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

/**
 * 跨会话记忆。
 *
 * 原来的对话历史只活在 native 内存里，App 一重启就全没了 —— 也就是说它“记不住”你。
 * 这里把每次要发出去的消息列表落盘，并在下次请求时把「上次记住的尾部」
 * 和本次会话无缝拼接（按重复消息去重），于是：
 *
 *   * 重启 App 后，它依然记得你们之前聊过什么
 *   * 同一会话内不会重复发送历史
 *   * 只保留最近 {@link #KEEP} 条，文件不会无限膨胀
 *
 * 只保留 role=user / assistant 的纯文本消息；带 tool_calls 的 assistant 消息和
 * role=tool 的结果会被丢掉 —— 否则把工具调用回放给模型会报 "tool_calls without response"。
 */
public final class AiMemory {

    private static final String DIR  = "/data/data/com.u3dexample/files";
    private static final String FILE = DIR + "/ai_memory.json";
    private static final int    KEEP = 40;          // 最多记 40 条（约 20 轮对话）
    private static final Object LOCK = new Object();

    private AiMemory() {}

    /** 对外唯一入口：注入记忆 + 落盘，返回新的 payload。 */
    public static String enrich(String payload) {
        synchronized (LOCK) {
            try {
                JSONObject root = new JSONObject(payload);
                JSONArray cur = root.optJSONArray("messages");
                if (cur == null) return payload;

                // 1. 拆出 system（不进记忆）与可记忆的对话消息
                java.util.List<JSONObject> sys = new java.util.ArrayList<JSONObject>();
                java.util.List<JSONObject> now = new java.util.ArrayList<JSONObject>();
                for (int i = 0; i < cur.length(); i++) {
                    JSONObject m = cur.optJSONObject(i);
                    if (m == null) continue;
                    String role = m.optString("role");
                    if ("system".equals(role)) { sys.add(m); continue; }
                    if ("tool".equals(role)) continue;
                    if ("assistant".equals(role) && m.has("tool_calls")) continue;
                    if (m.optString("content").length() == 0) continue;
                    now.add(m);
                }

                // 2. 记忆尾部 + 本次新增（按重复消息去重）
                JSONArray saved = read();
                java.util.List<JSONObject> out = new java.util.ArrayList<JSONObject>();
                for (int i = 0; i < saved.length(); i++) {
                    JSONObject o = saved.optJSONObject(i);
                    if (o != null) out.add(o);
                }
                int from = 0;
                if (saved.length() > 0 && !now.isEmpty()) {
                    JSONObject tail = out.get(out.size() - 1);
                    for (int i = now.size() - 1; i >= 0; i--) {
                        if (same(now.get(i), tail)) { from = i + 1; break; }
                    }
                }
                for (int i = from; i < now.size(); i++) out.add(now.get(i));

                // 3. 只留最近 KEEP 条并落盘
                int start = Math.max(0, out.size() - KEEP);
                JSONArray keep = new JSONArray();
                for (int i = start; i < out.size(); i++) keep.put(out.get(i));
                write(keep);

                // 4. 回填：system 原样，对话部分换成「记忆 + 本次」
                JSONArray fin = new JSONArray();
                for (int i = 0; i < sys.size(); i++) fin.put(sys.get(i));
                for (int i = start; i < out.size(); i++) fin.put(out.get(i));
                root.put("messages", fin);

                if (sys.size() == 0) root.remove("messages"); // 理论上不会发生
                if (fin.length() == 0) root.put("messages", cur);

                return root.toString();
            } catch (Throwable e) {
                return payload;   // 记忆坏了也绝不能影响正常对话
            }
        }
    }

    /** 清空记忆（供以后接「清空」按钮用）。 */
    public static void clear() {
        synchronized (LOCK) {
            try { new File(FILE).delete(); } catch (Throwable ignored) { }
        }
    }

    // -----------------------------------------------------------------

    private static boolean same(JSONObject a, JSONObject b) {
        return a.optString("role").equals(b.optString("role"))
                && a.optString("content").equals(b.optString("content"));
    }

    private static JSONArray read() {
        try {
            File f = new File(FILE);
            if (!f.exists() || f.length() <= 2) return new JSONArray();
            byte[] b = new byte[(int) f.length()];
            FileInputStream in = new FileInputStream(f);
            int n = in.read(b);
            in.close();
            return new JSONArray(new String(b, 0, Math.max(0, n), "UTF-8"));
        } catch (Throwable e) {
            return new JSONArray();
        }
    }

    private static void write(JSONArray a) {
        try {
            new File(DIR).mkdirs();
            FileOutputStream o = new FileOutputStream(FILE);
            o.write(a.toString().getBytes("UTF-8"));
            o.flush();
            o.close();
        } catch (Throwable ignored) { }
    }
}