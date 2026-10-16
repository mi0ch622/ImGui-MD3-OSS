package com.u3dexample;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * MCP 客户端（Streamable HTTP 传输）。
 *
 * 覆盖真实的开源 MCP 服务：MT管理器 的本地 MCP、DeepWiki、Context7 …
 * 协议：JSON-RPC 2.0 / initialize -> notifications/initialized -> tools/list -> tools/call
 * 响应同时支持 application/json 与 text/event-stream（SSE）。
 *
 * 说明：stdio 型 MCP（npx / uvx 起的 server）需要 Node/Python 运行时与容器，
 * 本 App 无容器，暂不支持，仅保留字段。
 */
public final class McpManager {

    public static final String TOOL_PREFIX = "mcp__";
    private static final long TOOLS_TTL_MS = 5 * 60 * 1000L;

    private static Context sCtx;
    private static int sIdSeq = 0;

    private McpManager() { }

    public static void attach(Context c) { sCtx = c.getApplicationContext(); }

    // =================================================================
    //  数据模型
    // =================================================================
    public static final class Server {
        public String  id      = "";
        public String  name    = "";
        public String  url     = "";
        public String  header  = "";     // 每行 "Key: Value"
        public boolean enabled = false;
        public String  sessionId = null; // 运行期
        public boolean ready     = false;
        public long    toolsAt   = 0L;
        public JSONArray tools   = null;
    }

    private static final List<Server> sServers = new ArrayList<Server>();

    // =================================================================
    //  配置读写（存 SharedPreferences，以 JSON 暴露给 C++）
    // =================================================================
    public static synchronized List<Server> servers() {
        if (sServers.isEmpty()) loadFromPrefs();
        return sServers;
    }

    private static void loadFromPrefs() {
        sServers.clear();
        String raw = WorkspaceManager.cfgGet("mcp_servers", "");
        if (raw == null || raw.length() == 0) return;
        try {
            JSONArray arr = new JSONArray(raw);
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                Server s = new Server();
                s.id      = o.optString("id", "s" + i);
                s.name    = o.optString("name", s.id);
                s.url     = o.optString("url", "");
                s.header  = o.optString("header", "");
                s.enabled = o.optBoolean("enabled", false);
                sServers.add(s);
            }
        } catch (Throwable ignored) { }
    }

    public static synchronized void saveServers(JSONArray arr) {
        if (arr == null) return;
        try {
            WorkspaceManager.cfgPut("mcp_servers", arr.toString());
            sServers.clear();
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                Server s = new Server();
                s.id      = o.optString("id", "s" + i);
                s.name    = o.optString("name", s.id);
                s.url     = o.optString("url", "");
                s.header  = o.optString("header", "");
                s.enabled = o.optBoolean("enabled", false);
                sServers.add(s);
            }
        } catch (Throwable ignored) { }
    }

    public static synchronized String serversJson() {
        JSONArray arr = new JSONArray();
        List<Server> list = servers();
        for (int i = 0; i < list.size(); i++) {
            Server s = list.get(i);
            JSONObject o = new JSONObject();
            try {
                o.put("id", s.id);
                o.put("name", s.name);
                o.put("url", s.url);
                o.put("header", s.header);
                o.put("enabled", s.enabled);
            } catch (Throwable ignored) { }
            arr.put(o);
        }
        return arr.toString();
    }

    public static synchronized Server byId(String id) {
        for (Server s : servers()) if (s.id.equals(id)) return s;
        return null;
    }

    public static int count() { return servers().size(); }

    // =================================================================
    //  内置预设（真实可用的开源 / 公开 MCP 服务）
    // =================================================================
    public static JSONArray presets() {
        JSONArray arr = new JSONArray();
        arr.put(preset("deepwiki", "DeepWiki",
                "https://mcp.deepwiki.com/mcp",
                "把任意 GitHub 仓库变成可问答的 Wiki，读源码结构、查实现细节。",
                "公开服务，无需 Key"));
        arr.put(preset("context7", "Context7",
                "https://mcp.context7.com/mcp",
                "查询各种开源库的最新官方文档与代码示例。",
                "公开服务，无需 Key"));
        arr.put(preset("mt", "MT管理器（本地）",
                "http://127.0.0.1:8787/mcp",
                "调用 MT管理器 自身暴露的 MCP 服务，直接用文件管理能力读写设备文件。",
                "默认地址已按 MT管理器 填写：http://127.0.0.1:8787/mcp"));
        arr.put(preset("amap", "高德地图",
                "https://mcp.amap.com/mcp?key=你的KEY",
                "地理编码、路径规划、POI 搜索。",
                "把 URL 里的 你的KEY 换成高德开放平台的 Web 服务 Key"));
        arr.put(preset("ssh", "SSH",
                "http://127.0.0.1:8787/mcp",
                "自定义：名称与地址都可以自己改，填好后点「保存」即可。",
                "名称默认叫 SSH，地址默认 http://127.0.0.1:8787/mcp，都可以自己改"));
        arr.put(preset("custom", "自定义（自己填写）",
                "",
                "名称与地址完全自己填写。",
                "添加后会自动弹出编辑框，填名称和地址"));
        return arr;
    }

    private static JSONObject preset(String id, String name, String url, String desc, String hint) {
        JSONObject o = new JSONObject();
        try {
            o.put("id", id);
            o.put("name", name);
            o.put("url", url);
            o.put("desc", desc);
            o.put("hint", hint);
        } catch (Throwable ignored) { }
        return o;
    }

    // =================================================================
    //  传输层
    // =================================================================
    private static JSONObject rpc(Server s, String method, JSONObject params, boolean notify) throws Exception {
        int id = ++sIdSeq;
        JSONObject req = new JSONObject();
        req.put("jsonrpc", "2.0");
        if (!notify) req.put("id", id);
        req.put("method", method);
        if (params != null) req.put("params", params);

        HttpURLConnection conn = (HttpURLConnection) new URL(s.url).openConnection();
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Content-Type", "application/json");
        conn.setRequestProperty("Accept", "application/json, text/event-stream");
        conn.setRequestProperty("User-Agent", "Avates-MCP/1.0");
        if (s.header != null && s.header.length() > 0) {
            for (String line : s.header.split("\\n")) {
                int c = line.indexOf(':');
                if (c > 0) {
                    conn.setRequestProperty(line.substring(0, c).trim(), line.substring(c + 1).trim());
                }
            }
        }
        if (s.sessionId != null && s.sessionId.length() > 0) {
            conn.setRequestProperty("Mcp-Session-Id", s.sessionId);
        }
        conn.setConnectTimeout(4000);
        conn.setReadTimeout(12000);
        conn.setDoOutput(true);

        OutputStream os = conn.getOutputStream();
        os.write(req.toString().getBytes("UTF-8"));
        os.flush();
        os.close();

        int code = conn.getResponseCode();
        String sid = conn.getHeaderField("Mcp-Session-Id");
        if (sid != null && sid.length() > 0) s.sessionId = sid;

        InputStream in = (code >= 200 && code < 300) ? conn.getInputStream() : conn.getErrorStream();
        String body = readAll(in);
        try { conn.disconnect(); } catch (Throwable ignored) { }

        if (code < 200 || code >= 300) {
            throw new Exception("HTTP " + code + "：" + head(body, 220));
        }
        if (notify) return null;
        return parseRpc(body, id);
    }

    private static JSONObject parseRpc(String body, int id) throws Exception {
        if (body == null) throw new Exception("空响应");
        String t = body.trim();
        if (t.length() == 0) throw new Exception("空响应");

        if (t.charAt(0) == '{') {
            JSONObject o = new JSONObject(t);
            return o;
        }

        // SSE：逐段解析 data: 行，取 id 匹配的那条
        JSONObject found = null;
        StringBuilder data = new StringBuilder();
        String[] lines = t.split("\n");
        for (int i = 0; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.startsWith("data:")) {
                data.append(line.substring(5).trim());
            } else if (line.length() == 0 && data.length() > 0) {
                JSONObject o = tryJson(data.toString());
                if (o != null && (o.has("result") || o.has("error"))) {
                    if (o.optInt("id", -1) == id) return o;
                    if (found == null) found = o;
                }
                data.setLength(0);
            }
        }
        if (data.length() > 0) {
            JSONObject o = tryJson(data.toString());
            if (o != null && found == null) found = o;
        }
        if (found == null) throw new Exception("无法解析 MCP 响应：" + head(body, 200));
        return found;
    }

    private static JSONObject tryJson(String s) {
        try { return new JSONObject(s); } catch (Throwable e) { return null; }
    }

    private static String readAll(InputStream in) {
        if (in == null) return "";
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            byte[] buf = new byte[8192];
            int n, total = 0;
            while ((n = in.read(buf)) > 0) {
                bos.write(buf, 0, n);
                total += n;
                if (total > 512 * 1024) break;
            }
            in.close();
        } catch (Throwable ignored) { }
        try { return bos.toString("UTF-8"); } catch (Throwable e) { return ""; }
    }

    private static String head(String s, int n) {
        if (s == null) return "";
        return s.length() <= n ? s : s.substring(0, n) + "…";
    }

    // =================================================================
    //  协议动作
    // =================================================================
    private static void ensureReady(Server s) throws Exception {
        if (s.ready) return;
        JSONObject p = new JSONObject();
        p.put("protocolVersion", "2024-11-05");
        p.put("capabilities", new JSONObject());
        JSONObject ci = new JSONObject();
        ci.put("name", "Avates");
        ci.put("version", "1.0");
        p.put("clientInfo", ci);
        rpc(s, "initialize", p, false);
        try { rpc(s, "notifications/initialized", new JSONObject(), true); } catch (Throwable ignored) { }
        s.ready = true;
    }

    /** 拉取某个 server 的工具列表（带 5 分钟缓存）。失败返回 null。 */
    public static synchronized JSONArray toolsOf(Server s) {
        if (s == null) return null;
        long now = System.currentTimeMillis();
        if (s.tools != null && (now - s.toolsAt) < TOOLS_TTL_MS) return s.tools;
        try {
            ensureReady(s);
            JSONObject r = rpc(s, "tools/list", new JSONObject(), false);
            JSONObject res = r.optJSONObject("result");
            if (res == null) return null;
            s.tools = res.optJSONArray("tools");
            s.toolsAt = now;
            return s.tools;
        } catch (Throwable e) {
            s.ready = false;
            s.sessionId = null;
            s.tools = null;
            return null;
        }
    }

    /** 调用某个 server 的工具，返回纯文本结果。 */
    public static String callTool(Server s, String toolName, String argsJson) {
        try {
            ensureReady(s);
            JSONObject p = new JSONObject();
            p.put("name", toolName);
            p.put("arguments", new JSONObject(argsJson == null || argsJson.length() == 0 ? "{}" : argsJson));
            JSONObject r = rpc(s, "tools/call", p, false);
            if (r.has("error") && !r.isNull("error")) {
                JSONObject err = r.optJSONObject("error");
                return "MCP 返回错误：" + (err == null ? r.optString("error") : err.optString("message"));
            }
            return extractText(r.optJSONObject("result"));
        } catch (Throwable e) {
            return "MCP 调用失败：" + e.getMessage();
        }
    }

    private static String extractText(JSONObject result) {
        if (result == null) return "（MCP 无返回）";
        StringBuilder sb = new StringBuilder();
        JSONArray content = result.optJSONArray("content");
        if (content != null) {
            for (int i = 0; i < content.length(); i++) {
                JSONObject c = content.optJSONObject(i);
                if (c == null) continue;
                String type = c.optString("type", "");
                if ("text".equals(type)) sb.append(c.optString("text", ""));
                else if ("resource".equals(type)) sb.append(c.optString("uri", "[resource]"));
                else sb.append("[").append(type).append("]");
                sb.append("\n");
            }
        }
        if (sb.length() == 0) sb.append(result.toString());
        if (result.optBoolean("isError", false)) sb.insert(0, "[工具报告错误] ");
        return sb.toString();
    }

    /** 设置页「测试连接」：拉一次 tools/list，返回描述文本。 */
    public static String test(Server s) {
        try {
            s.ready = false;
            s.sessionId = null;
            s.tools = null;
            ensureReady(s);
            JSONObject r = rpc(s, "tools/list", new JSONObject(), false);
            JSONObject res = r.optJSONObject("result");
            JSONArray tools = res == null ? null : res.optJSONArray("tools");
            int n = tools == null ? 0 : tools.length();
            s.tools = tools;
            s.toolsAt = System.currentTimeMillis();
            return "连接成功，发现 " + n + " 个工具";
        } catch (Throwable e) {
            return friendlyErr(e, s.url);
        }
    }

    /** 把底层异常翻成能照着修的人话 */
    private static String friendlyErr(Throwable e, String url) {
        String em = e == null ? "" : e.toString();
        String m  = e == null ? "" : String.valueOf(e.getMessage());
        String host = url == null ? "" : url;
        try {
            java.net.URL u = new java.net.URL(url);
            host = u.getHost() + ":" + u.getPort();
        } catch (Throwable ignored) { }

        if (em.contains("CLEARTEXT") || em.contains("Cleartext")) {
            return "明文 HTTP 被系统拦截（CLEARTEXT not permitted）：本版本已放行，请确认装的是最新包。";
        }
        if (em.contains("Connection refused") || em.contains("ECONNREFUSED")) {
            return "连不上（端口没开）：请确认 " + host + " 上的 MCP 服务已经启动。";
        }
        if (em.contains("SocketTimeout") || em.contains("timed out")) {
            return "连接超时：" + host + " 无响应，服务可能没开或被系统限制后台联网。";
        }
        if (m.contains("HTTP 404") || m.contains("HTTP404")) {
            return "HTTP 404：路径不对，endpoint 一般要带 /mcp（如 http://127.0.0.1:8787/mcp）。";
        }
        if (m.contains("HTTP 401") || m.contains("HTTP 403")) {
            return m + "（需要鉴权：点「编辑」，在 Header 里填 Authorization: Bearer <token>）";
        }
        if (m.contains("HTTP 4") || m.contains("HTTP 5")) return m;
        if (em.contains("SSL") || em.contains("Certificate")) return "TLS 失败：" + m;
        if (em.contains("Unable to resolve host")) return "域名解析失败：检查地址是否写对、设备是否联网。";
        return m.length() == 0 ? em : m;
    }

    /** 把工具名编码成 mcp__<serverId>__<toolName> */
    public static String encodeTool(String serverId, String tool) {
        return TOOL_PREFIX + serverId + "__" + tool;
    }

    // =================================================================
    //  给 C++ 设置界面用的索引式访问（避免在 C++ 里解析 JSON）
    // =================================================================
    public static String field(int i, String key) {
        List<Server> l = servers();
        if (i < 0 || i >= l.size()) return "";
        Server s = l.get(i);
        if ("id".equals(key))   return s.id;
        if ("name".equals(key)) return s.name;
        if ("url".equals(key))  return s.url;
        if ("tools".equals(key)) {
            JSONArray t = s.tools;
            return t == null ? "" : (t.length() + " 个工具");
        }
        return "";
    }

    public static boolean isEnabled(int i) {
        List<Server> l = servers();
        return (i >= 0 && i < l.size()) && l.get(i).enabled;
    }

    public static void setEnabled(int i, boolean v) {
        List<Server> l = servers();
        if (i < 0 || i >= l.size()) return;
        l.get(i).enabled = v;
        persist();
    }

    /** 改名 / 改地址（设置页「编辑」保存） */
    public static void update(int i, String name, String url) {
        List<Server> l = servers();
        if (i < 0 || i >= l.size()) return;
        Server s = l.get(i);
        if (name != null) s.name = name.trim();
        if (url != null)  s.url  = url.trim();
        s.ready = false;
        s.sessionId = null;
        s.tools = null;
        persist();
    }

    public static void add(String name, String url) {
        Server s = new Server();
        s.id = "s" + System.currentTimeMillis();
        s.name = (name == null || name.length() == 0) ? "自定义 MCP" : name;
        s.url = url == null ? "" : url;
        s.enabled = true;
        servers().add(s);
        persist();
    }

    public static void addPreset(String presetId) {
        JSONArray arr = presets();
        for (int i = 0; i < arr.length(); i++) {
            JSONObject o = arr.optJSONObject(i);
            if (o == null) continue;
            if (presetId.equals(o.optString("id", ""))) {
                add(o.optString("name", presetId), o.optString("url", ""));
                return;
            }
        }
        add(presetId, "");
    }

    public static void remove(int i) {
        List<Server> l = servers();
        if (i < 0 || i >= l.size()) return;
        l.remove(i);
        persist();
    }

    public static String nameOf(int i)  { return field(i, "name"); }
    public static String urlOf(int i)   { return field(i, "url"); }

    private static void persist() {
        saveServers(toJsonArray());
    }

    /**
     * 把当前列表序列化成 JSONArray。
     * 注意：绝不能写成 new JSONArray(servers()) —— Server 是自定义类，
     * org.json 的 wrap() 对非 java./javax. 且有 classloader 的对象返回 null，
     * 结果会存成 "[null]"，重载后就是「已配置 0 个」。
     */
    private static JSONArray toJsonArray() {
        JSONArray arr = new JSONArray();
        List<Server> l = servers();
        for (int i = 0; i < l.size(); i++) {
            Server s = l.get(i);
            JSONObject o = new JSONObject();
            try {
                o.put("id", s.id);
                o.put("name", s.name);
                o.put("url", s.url);
                o.put("header", s.header);
                o.put("enabled", s.enabled);
            } catch (Throwable ignored) { }
            arr.put(o);
        }
        return arr;
    }

    /** 设置页「测试连接」：后台线程跑，结果回调 native，绝不阻塞渲染线程 */
    public static void testAsync(final int i) {
        final Server s = (i >= 0 && i < servers().size()) ? servers().get(i) : null;
        if (s == null) return;
        new Thread(new Runnable() {
            @Override public void run() {
                String msg = test(s);
                nativeOnMcpTest(i, msg);
            }
        }, "mcp-test").start();
    }

    public static native void nativeOnMcpTest(int index, String message);

    public static int presetCount() { return presets().length(); }

    public static String presetField(int i, String key) {
        JSONObject o = presets().optJSONObject(i);
        return o == null ? "" : o.optString(key, "");
    }

    public static Server serverOfEncoded(String encoded) {
        if (encoded == null || !encoded.startsWith(TOOL_PREFIX)) return null;
        String rest = encoded.substring(TOOL_PREFIX.length());
        int i = rest.indexOf("__");
        if (i <= 0) return null;
        return byId(rest.substring(0, i));
    }

    public static String toolOfEncoded(String encoded) {
        if (encoded == null || !encoded.startsWith(TOOL_PREFIX)) return "";
        String rest = encoded.substring(TOOL_PREFIX.length());
        int i = rest.indexOf("__");
        return i < 0 ? "" : rest.substring(i + 2);
    }
}