package com.u3dexample;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *
 * C++ 侧把整段对话(含 system 提示词)以 JSON 传给 {@link #start(String)}，
 * 本类在后台线程完成：
 *   1. 以 stream=true 调用 DeepSeek Chat Completions
 *   2. 解析 SSE，逐段把 delta 文本回调给 native（流式输出）
 *   3. 识别 tool_calls -> 执行 web_search / fetch_url（真实联网）
 *      -> 把结果作为 role=tool 消息回灌 -> 继续推理，最多 4 轮
 *
 * 所有 UI 文本都通过 native 回调回到渲染线程，本类不直接触碰 UI。
 */
public final class AiChatBridge {

    private static final String API_URL       = "https://api.agnes-ai.cn/v1/chat/completions";

    /**
     * 多 Key 池。
     *
     * 平台把免费用户 RPM 下调 50%（=10 次/分钟），单 key 很容易被限流。
     * 这里把多个 key 组成一个池，并对每个 key 单独做令牌桶节流
     * （每个 key 最多 9 次/分钟，留 1 次余量），于是总可用速率 ≈ 9 × key数。
     * 请求永远发给「最久没被用过」的那个 key，天然均摊，不会触发 429。
     */
    private static final String[] API_KEYS = new String[] {
            "YOUR_API_KEY_HERE",
            "YOUR_API_KEY_HERE",
    };

    /** 每个 key 每分钟最多用几次（平台限制 10，留 1 次余量） */
    private static final int  RPM_PER_KEY = 9;
    /** 单个 key 的两次使用最小间隔（毫秒） */
    private static final long KEY_MIN_GAP = 60000L / RPM_PER_KEY;

    private static final long[] sKeyNextAt = new long[API_KEYS.length];
    private static int sKeyRR = 0;

    /**
     * 取一个当前可用的 key；若所有 key 都在冷却，则排队等待（可被取消打断）。
     * 返回 null 表示用户在排队期间点了停止。
     */
    private static synchronized String acquireKey() {
        for (int guard = 0; guard < 240; guard++) {
            long now = System.currentTimeMillis();
            int best = -1;
            long bestAt = Long.MAX_VALUE;
            // 优先取“已经冷却完”的；都没有就取最早可用的那个
            for (int i = 0; i < API_KEYS.length; i++) {
                int idx = (sKeyRR + i) % API_KEYS.length;
                if (sKeyNextAt[idx] <= now) { best = idx; bestAt = now; break; }
                if (sKeyNextAt[idx] < bestAt) { bestAt = sKeyNextAt[idx]; best = idx; }
            }
            long wait = sKeyNextAt[best] - now;
            if (wait <= 0) {
                sKeyNextAt[best] = now + KEY_MIN_GAP;
                sKeyRR = (best + 1) % API_KEYS.length;
                return API_KEYS[best];
            }
            // 全部在冷却 -> 提示排队，500ms 粒度轮询，等待期间可被取消
            if (sCancel) return null;
            nativeOnStatus("触到平台限流，排队中… 约 " + Math.max(1, wait / 1000) + " 秒");
            try { Thread.sleep(Math.min(wait, 500)); } catch (InterruptedException e) { return null; }
            if (sCancel) return null;
        }
        return API_KEYS[0];
    }

    /** 某个 key 返回 429 时，把它单独往后压一会儿 */
    private static synchronized void penalizeKey(String key) {
        for (int i = 0; i < API_KEYS.length; i++) {
            if (API_KEYS[i].equals(key)) {
                sKeyNextAt[i] = System.currentTimeMillis() + 20000L;
                return;
            }
        }
    }

    /** agnes 免费模型, 按顺序降级尝试 */
    private static final String[] MODELS = new String[] {
            "agnes-3.0-flash",
            "agnes-2.5-flash",
            "agnes-2.0-flash",
    };

    private static final String UA =
            "Mozilla/5.0 (Linux; Android 13; Mobile) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36";

    private static final int MAX_TOOL_ROUNDS = 4;

    // ===== native =====
    public static native void nativeOnDelta(String text);
    public static native void nativeOnStatus(String text);
    public static native void nativeOnNote(String text);
    public static native void nativeOnDone();
    public static native void nativeOnCancelled();
    public static native void nativeOnError(String message);

    private static volatile Thread          sThread;
    private static volatile boolean         sCancel;
    private static volatile boolean         sActive;   // 有一次请求正在飞行中
    private static volatile HttpURLConnection sConn;

    /** 暴露给模型的 MCP 工具名 -> {serverId, 真实工具名} */
    private static final Map<String, String[]> sMcpMap = new HashMap<String, String[]>();

    private AiChatBridge() {}

    // =================================================================
    //  对外入口
    // =================================================================

    /** 启动一次对话请求（payload 形如 {"tools":true,"messages":[...]}）。 */
    public static void start(final String payload0) {
        // 跨会话记忆: 注入历史 + 落盘（内部已做异常兜底，坏了也不影响对话）
        final String payload = AiMemory.enrich(payload0);
        // 静默终止上一次请求：不回调 native，避免和刚开始的新请求抢状态
        sCancel = true;
        HttpURLConnection oldConn = sConn;
        if (oldConn != null) {
            try { oldConn.disconnect(); } catch (Throwable ignored) { }
        }
        sConn = null;
        sCancel = false;
        sActive = true;

        Thread t = new Thread(new Runnable() {
            @Override public void run() {
                try {
                    runLoop(payload);
                } catch (Throwable e) {
                    if (!sCancel) nativeOnError(friendly(e));
                } finally {
                    // 只有自己仍是最新的那次请求时才清状态（避免覆盖下一次请求）
                    if (sThread == Thread.currentThread()) {
                        sConn = null;
                        sThread = null;
                        sActive = false;
                    }
                }
            }
        }, "ai-chat");
        sThread = t;
        t.start();
    }

    /**
     * 用户点“停止”。
     * 立即切断连接并**马上**回调 nativeOnCancelled —— 不等读阻塞解除，
     * 否则 UI 会一直卡在“停止”按钮上（这就是之前点了停不下来的原因）。
     */
    public static void cancel() {
        boolean was = sActive;
        sCancel = true;
        sActive = false;
        HttpURLConnection c = sConn;
        if (c != null) {
            try { c.disconnect(); } catch (Throwable ignored) { }
        }
        if (was) {
            // 关键：cancel() 可能是被「渲染线程」同步调进来的（C++ 里点“停止”按钮），
            // 因此必须丢到独立线程里去通知。
            new Thread(new Runnable() {
                @Override public void run() { nativeOnCancelled(); }
            }, "ai-cancel").start();
        }
    }

    // =================================================================
    //  主循环（含工具调用轮次）
    // =================================================================

    private static void runLoop(String payload) throws Exception {
        JSONObject root = new JSONObject(payload);
        boolean useTools = root.optBoolean("tools", true);
        JSONArray in = root.optJSONArray("messages");

        List<JSONObject> msgs = new ArrayList<JSONObject>();
        if (in != null) {
            for (int i = 0; i < in.length(); i++) msgs.add(in.getJSONObject(i));
        }

        for (int round = 0; round < MAX_TOOL_ROUNDS; round++) {
            if (sCancel) return;

            String model = MODELS[0];
            StreamResult res = null;

            ApiError lastErr = null;
            for (int mi = 0; mi < MODELS.length; mi++) {
                model = MODELS[mi];
                try {
                    res = streamOnce(model, msgs, useTools);
                    lastErr = null;
                    break;
                } catch (ApiError ae) {
                    lastErr = ae;
                    boolean modelProblem = ae.body != null
                            && ae.body.toLowerCase().contains("model")
                            && ae.code >= 400 && ae.code < 500;
                    if (modelProblem) continue;      // 换下一个免费模型
                    throw ae;
                }
            }
            if (lastErr != null) throw lastErr;
            if (res == null || sCancel) return;

            if (res.toolCalls.isEmpty()) {
                nativeOnDone();
                return;
            }

            // ---- 组装 assistant(tool_calls) 消息 ----
            JSONObject am = new JSONObject();
            am.put("role", "assistant");
            if (res.content.length() > 0) am.put("content", res.content.toString());
            else am.put("content", JSONObject.NULL);

            JSONArray tcs = new JSONArray();
            for (int i = 0; i < res.toolCalls.size(); i++) {
                ToolCall tc = res.toolCalls.get(i);
                JSONObject o = new JSONObject();
                o.put("id", tc.id.isEmpty() ? ("call_" + System.nanoTime() + "_" + i) : tc.id);
                o.put("type", "function");
                JSONObject fn = new JSONObject();
                fn.put("name", tc.name);
                fn.put("arguments", tc.args.length() == 0 ? "{}" : tc.args.toString());
                o.put("function", fn);
                tcs.put(o);
                if (tc.id.isEmpty()) tc.id = o.getString("id");
            }
            am.put("tool_calls", tcs);
            msgs.add(am);

            // ---- 依次执行工具 ----
            for (int i = 0; i < res.toolCalls.size(); i++) {
                if (sCancel) return;
                ToolCall tc = res.toolCalls.get(i);

                String query = "";
                try { query = new JSONObject(tc.args.toString()).optString("query", ""); }
                catch (Throwable ignored) { }

                if ("web_search".equals(tc.name)) {
                    // 注: 不要用 emoji —— 字体图集里没有这些字形, 会渲染成 "?"
                    nativeOnNote("联网搜索：" + (query.isEmpty() ? "(空)" : query));
                    nativeOnStatus("正在联网搜索…");
                } else {
                    nativeOnNote("调用工具：" + tc.name);
                    nativeOnStatus("正在获取网页…");
                }

                String out = execTool(tc.name, tc.args.toString());

                JSONObject tm = new JSONObject();
                tm.put("role", "tool");
                tm.put("tool_call_id", tc.id);
                tm.put("content", out);
                msgs.add(tm);
            }
            nativeOnStatus("正在整理答案…");
        }

        nativeOnDone();
    }

    // =================================================================
    //  单次流式请求
    // =================================================================

    private static StreamResult streamOnce(String model, List<JSONObject> msgs, boolean useTools)
            throws Exception {

        JSONObject body = new JSONObject();
        body.put("model", model);
        body.put("stream", true);
        body.put("temperature", 0.7);
        body.put("max_tokens", 16384);

        JSONArray arr = new JSONArray();
        for (int i = 0; i < msgs.size(); i++) arr.put(msgs.get(i));
        body.put("messages", arr);

        if (useTools) {
            body.put("tools", buildTools());
            body.put("tool_choice", "auto");
        }

        byte[] data = body.toString().getBytes("UTF-8");

        String useKey = acquireKey();
        if (useKey == null) throw new RuntimeException("已取消");

        HttpURLConnection conn = (HttpURLConnection) new URL(API_URL).openConnection();
        sConn = conn;
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Content-Type", "application/json; charset=utf-8");
        conn.setRequestProperty("Authorization", "Bearer " + useKey);
        conn.setRequestProperty("Accept", "text/event-stream");
        conn.setRequestProperty("User-Agent", "Avates-ImGui/1.0");
        conn.setConnectTimeout(20000);
        conn.setReadTimeout(150000);
        conn.setDoOutput(true);

        OutputStream os = conn.getOutputStream();
        os.write(data);
        os.flush();
        os.close();

        int code = conn.getResponseCode();
        if (code < 200 || code >= 300) {
            String err = readAll(conn.getErrorStream());
            try { conn.disconnect(); } catch (Throwable ignored) { }
            throw new ApiError(code, err);
        }

        StreamResult res = new StreamResult();
        BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), "UTF-8"));
        try {
            String line;
            while ((line = br.readLine()) != null) {
                if (sCancel) break;
                if (line.length() == 0) continue;
                if (!line.startsWith("data:")) continue;

                String chunk = line.substring(5).trim();
                if (chunk.equals("[DONE]")) break;
                if (chunk.isEmpty()) continue;

                JSONObject j;
                try { j = new JSONObject(chunk); } catch (Throwable ignored) { continue; }

                JSONArray choices = j.optJSONArray("choices");
                if (choices == null || choices.length() == 0) continue;

                JSONObject c0 = choices.getJSONObject(0);
                if (c0.has("finish_reason") && !c0.isNull("finish_reason")) {
                    res.finishReason = c0.optString("finish_reason", "");
                }

                JSONObject d = c0.optJSONObject("delta");
                if (d == null) continue;

                if (d.has("content") && !d.isNull("content")) {
                    String piece = d.optString("content", "");
                    if (piece.length() > 0) {
                        if (!res.sawContent) {
                            res.sawContent = true;
                            nativeOnStatus("");          // 开始输出正文, 清掉“思考中…”
                        }
                        res.content.append(piece);
                        nativeOnDelta(piece);
                    }
                }

                // 推理模型(deepseek-flash)会先流 reasoning_content, 这里只用来显示“思考中…”
                if (d.has("reasoning_content") && !d.isNull("reasoning_content")) {
                    String think = d.optString("reasoning_content", "");
                    if (think.length() > 0 && !res.sawReasoning) {
                        res.sawReasoning = true;
                        nativeOnStatus("思考中…");
                    }
                }

                if (d.has("tool_calls") && !d.isNull("tool_calls")) {
                    JSONArray tcs = d.optJSONArray("tool_calls");
                    if (tcs != null) {
                        for (int i = 0; i < tcs.length(); i++) {
                            JSONObject tc = tcs.getJSONObject(i);
                            int idx = tc.optInt("index", i);
                            ToolCall t = res.getOrCreate(idx);
                            if (tc.has("id") && !tc.isNull("id")) {
                                String id = tc.optString("id", "");
                                if (id.length() > 0) t.id = id;
                            }
                            JSONObject fn = tc.optJSONObject("function");
                            if (fn != null) {
                                if (fn.has("name") && !fn.isNull("name")) {
                                    String nm = fn.optString("name", "");
                                    if (nm.length() > 0) t.name = nm;
                                }
                                if (fn.has("arguments") && !fn.isNull("arguments")) {
                                    t.args.append(fn.optString("arguments", ""));
                                }
                            }
                        }
                    }
                }
            }
        } finally {
            try { br.close(); } catch (Throwable ignored) { }
            try { conn.disconnect(); } catch (Throwable ignored) { }
            sConn = null;
        }
        return res;
    }

    // =================================================================
    //  工具
    // =================================================================

    private static JSONArray buildTools() throws Exception {
        JSONArray tools = new JSONArray();

        JSONObject t1 = new JSONObject();
        t1.put("type", "function");
        JSONObject f1 = new JSONObject();
        f1.put("name", "web_search");
        f1.put("description",
                "在互联网上进行实时搜索，返回若干条结果的标题、链接与摘要。"
                + "当问题涉及新闻、时事、最新数据、价格、版本号、人物近况或任何你不确定的事实时，"
                + "必须先调用本工具，再基于搜索结果回答，并在答案中给出来源链接。");
        JSONObject p1 = new JSONObject();
        p1.put("type", "object");
        JSONObject props1 = new JSONObject();
        JSONObject q = new JSONObject();
        q.put("type", "string");
        q.put("description", "搜索关键词，尽量精确，例如“2025年诺贝尔物理学奖 得主”");
        props1.put("query", q);
        p1.put("properties", props1);
        p1.put("required", new JSONArray().put("query"));
        f1.put("parameters", p1);
        t1.put("function", f1);
        tools.put(t1);

        JSONObject t2 = new JSONObject();
        t2.put("type", "function");
        JSONObject f2 = new JSONObject();
        f2.put("name", "fetch_url");
        f2.put("description",
                "抓取一个网页的正文纯文本（最多 4000 字）。当搜索结果摘要不足、"
                + "需要查看详情页内容时使用。");
        JSONObject p2 = new JSONObject();
        p2.put("type", "object");
        JSONObject props2 = new JSONObject();
        JSONObject u = new JSONObject();
        u.put("type", "string");
        u.put("description", "要抓取的完整 http/https 链接");
        props2.put("url", u);
        p2.put("properties", props2);
        p2.put("required", new JSONArray().put("url"));
        f2.put("parameters", p2);
        t2.put("function", f2);
        tools.put(t2);

        // ===== 工作区文件工具 =====
        tools.put(makeTool("ws_list", "列出工作区目录内容，查看有哪些文件。",
                new String[]{"path"},
                new String[][]{{"path", "string", "工作区内相对路径，根目录填 . "}}));
        tools.put(makeTool("ws_read", "读取工作区里的文本文件内容（默认最多 64KB）。",
                new String[]{"path"},
                new String[][]{{"path", "string", "工作区内相对路径"},
                               {"maxBytes", "integer", "最多读取字节数，默认 65536"}}));
        tools.put(makeTool("ws_write", "把内容写入工作区文件（覆盖写，父目录自动创建）。"
                        + "用户要求写代码、写笔记、生成文档、改文件时用它。",
                new String[]{"path", "content"},
                new String[][]{{"path", "string", "工作区内相对路径，如 demo/main.py"},
                               {"content", "string", "完整文本内容"}}));
        tools.put(makeTool("ws_mkdir", "在工作区里创建目录。",
                new String[]{"path"},
                new String[][]{{"path", "string", "要创建的相对目录"}}));

        // ===== MCP 工具（从已启用的服务器动态拉取）=====
        sMcpMap.clear();
        List<McpManager.Server> srvList = McpManager.servers();
        for (int si = 0; si < srvList.size(); si++) {
            McpManager.Server srv = srvList.get(si);
            if (!srv.enabled) continue;
            JSONArray ts = McpManager.toolsOf(srv);
            if (ts == null) continue;
            for (int i = 0; i < ts.length() && i < 24; i++) {
                JSONObject t = ts.optJSONObject(i);
                if (t == null) continue;
                String real = t.optString("name", "");
                if (real.length() == 0) continue;
                String exposed = ("mcp_" + srv.id + "_" + i).replaceAll("[^a-zA-Z0-9_-]", "_");
                if (exposed.length() > 60) exposed = exposed.substring(0, 60);
                sMcpMap.put(exposed, new String[]{srv.id, real});
                tools.put(makeToolRaw(exposed,
                        "[MCP·" + srv.name + "] " + t.optString("description", real),
                        t.optJSONObject("inputSchema")));
            }
        }

        return tools;
    }

    /** 用显式 properties / required 组一个工具定义 */
    private static JSONObject makeTool(String name, String desc, String[] req, String[][] props)
            throws Exception {
        JSONObject pr = new JSONObject();
        for (int i = 0; i < props.length; i++) {
            JSONObject o = new JSONObject();
            o.put("type", props[i][1]);
            o.put("description", props[i][2]);
            pr.put(props[i][0], o);
        }
        JSONArray r = new JSONArray();
        for (int i = 0; i < req.length; i++) r.put(req[i]);
        JSONObject p = new JSONObject();
        p.put("type", "object");
        p.put("properties", pr);
        p.put("required", r);
        return wrapTool(name, desc, p);
    }

    /** 直接用 MCP 提供的 inputSchema 组工具定义 */
    private static JSONObject makeToolRaw(String name, String desc, JSONObject schema)
            throws Exception {
        if (schema == null) {
            schema = new JSONObject();
            schema.put("type", "object");
            schema.put("properties", new JSONObject());
        }
        return wrapTool(name, desc, schema);
    }

    private static JSONObject wrapTool(String name, String desc, JSONObject params) throws Exception {
        JSONObject t = new JSONObject();
        t.put("type", "function");
        JSONObject f = new JSONObject();
        f.put("name", name);
        f.put("description", desc);
        f.put("parameters", params);
        t.put("function", f);
        return t;
    }

    private static String execTool(String name, String argsJson) {
        try {
            JSONObject a = new JSONObject(argsJson.length() == 0 ? "{}" : argsJson);
            if ("web_search".equals(name))  return webSearch(a.optString("query", ""));
            if ("fetch_url".equals(name))   return fetchUrl(a.optString("url", ""));

            // ===== 工作区文件工具 =====
            if ("ws_list".equals(name))  return WorkspaceManager.listDir(a.optString("path", "."));
            if ("ws_read".equals(name))  return WorkspaceManager.readFile(a.optString("path", ""), a.optInt("maxBytes", 65536));
            if ("ws_write".equals(name)) return WorkspaceManager.writeFile(a.optString("path", ""), a.optString("content", ""));
            if ("ws_mkdir".equals(name)) return WorkspaceManager.makeDir(a.optString("path", ""));

            // ===== MCP 工具 =====
            String[] mcp = sMcpMap.get(name);
            if (mcp != null) {
                McpManager.Server srv = McpManager.byId(mcp[0]);
                if (srv == null) return "MCP 服务器不存在：" + mcp[0];
                return McpManager.callTool(srv, mcp[1], argsJson);
            }

            return "未知工具：" + name;
        } catch (Throwable e) {
            return "工具执行失败：" + e;
        }
    }

    private static String webSearch(String query) {
        if (query == null || query.trim().length() == 0) return "搜索词为空。";
        StringBuilder all = new StringBuilder();
        String[] engines = new String[] {
                "https://cn.bing.com/search?setlang=zh-CN&ensearch=0&q=",
                "https://www.bing.com/search?setmkt=zh-CN&q=",
        };
        for (int i = 0; i < engines.length; i++) {
            try {
                String html = httpGet(engines[i] + URLEncoder.encode(query, "UTF-8"));
                String parsed = parseResults(html);
                if (parsed.length() > 0) {
                    all.append("【联网搜索结果】").append(query).append("\n");
                    all.append(parsed);
                    break;
                }
            } catch (Throwable ignored) { }
        }
        if (all.length() == 0) {
            try {
                String html = httpGet("https://cn.bing.com/search?q=" + URLEncoder.encode(query, "UTF-8"));
                String txt = stripTags(html);
                if (txt.length() > 1200) txt = txt.substring(0, 1200);
                return "【联网结果（原始文本）】\n" + txt;
            } catch (Throwable e) {
                return "联网搜索失败：" + e;
            }
        }
        return all.toString();
    }

    /** 抓取 Bing/Baidu 风格结果页，抽取 标题 + 链接 + 摘要。 */
    private static String parseResults(String html) {
        StringBuilder sb = new StringBuilder();
        if (html == null) return "";

        Pattern block = Pattern.compile(
                "<li class=\"b_algo\"[\\s\\S]*?(?=<li class=\"b_algo\"|</ol>|</body>)");
        Matcher m = block.matcher(html);
        int n = 0;
        while (m.find() && n < 6) {
            String blk = m.group();
            String link = null, title = null, snip = null;

            Matcher a = Pattern.compile(
                    "<h2[^>]*>\\s*<a[^>]*href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>").matcher(blk);
            if (a.find()) {
                link = a.group(1);
                title = stripInline(a.group(2));
            } else {
                Matcher a2 = Pattern.compile("<a[^>]*href=\"(http[^\"]+)\"[^>]*>([\\s\\S]{0,120}?)</a>")
                        .matcher(blk);
                if (a2.find()) { link = a2.group(1); title = stripInline(a2.group(2)); }
            }
            Matcher p = Pattern.compile("<p[^>]*>([\\s\\S]*?)</p>").matcher(blk);
            if (p.find()) snip = stripInline(p.group(1));

            if (title == null || title.length() < 2) continue;
            n++;
            sb.append(n).append(". ").append(title).append("\n");
            if (link != null) sb.append("   链接：").append(link).append("\n");
            if (snip != null && snip.length() > 0) sb.append("   摘要：").append(snip).append("\n");
        }
        return sb.toString();
    }

    private static String fetchUrl(String url) {
        if (url == null || !(url.startsWith("http://") || url.startsWith("https://"))) {
            return "无效链接。";
        }
        try {
            String html = httpGet(url);
            String txt = stripTags(html);
            if (txt.length() > 4000) txt = txt.substring(0, 4000);
            return "【网页正文】" + url + "\n" + txt;
        } catch (Throwable e) {
            return "抓取失败：" + e;
        }
    }

    // =================================================================
    //  基础网络 / 文本工具
    // =================================================================

    private static String httpGet(String url) throws Exception {
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setRequestProperty("User-Agent", UA);
        conn.setRequestProperty("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        conn.setRequestProperty("Accept", "text/html,application/xhtml+xml,*/*");
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(25000);
        conn.setInstanceFollowRedirects(true);
        int code = conn.getResponseCode();
        InputStream in = (code >= 200 && code < 400) ? conn.getInputStream() : conn.getErrorStream();
        String s = readAll(in);
        try { conn.disconnect(); } catch (Throwable ignored) { }
        return s;
    }

    private static String readAll(InputStream in) {
        if (in == null) return "";
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        try {
            int n;
            int total = 0;
            while ((n = in.read(buf)) > 0) {
                bos.write(buf, 0, n);
                total += n;
                if (total > 900 * 1024) break;      // 上限 900KB
            }
        } catch (Throwable ignored) { }
        try { in.close(); } catch (Throwable ignored) { }
        try { return bos.toString("UTF-8"); } catch (Throwable e) { return ""; }
    }

    private static String stripInline(String s) {
        if (s == null) return "";
        String t = s.replaceAll("<[^>]+>", "");
        t = t.replace("&nbsp;", " ").replace("&amp;", "&").replace("&lt;", "<")
             .replace("&gt;", ">").replace("&" + "quot;", "\"").replace("&#39;", "'")
             .replace("&hellip;", "…").replace("&middot;", "·");
        t = t.replaceAll("\\s+", " ").trim();
        return t;
    }

    private static String stripTags(String html) {
        if (html == null) return "";
        String t = html;
        t = t.replaceAll("(?is)<script[^>]*>.*?</script>", " ");
        t = t.replaceAll("(?is)<style[^>]*>.*?</style>", " ");
        t = t.replaceAll("(?is)<head[^>]*>.*?</head>", " ");
        t = t.replaceAll("(?s)<!--.*?-->", " ");
        t = t.replaceAll("<[^>]+>", " ");
        t = t.replace("&nbsp;", " ").replace("&amp;", "&").replace("&lt;", "<")
             .replace("&gt;", ">").replace("&" + "quot;", "\"").replace("&#39;", "'");
        t = t.replaceAll("\\s+", " ").trim();
        return t;
    }

    private static String friendly(Throwable e) {
        String msg = e == null ? "未知错误" : e.toString();
        if (msg.contains("Unable to resolve host")) return "网络不可用：无法解析 api.deepseek.com";
        if (msg.contains("timed out") || msg.contains("timeout")) return "请求超时，请重试";
        if (msg.contains("SSL") || msg.contains("Certificate")) return "TLS 握手失败：" + msg;
        if (msg.length() > 220) msg = msg.substring(0, 220);
        return msg;
    }

    // =================================================================
    //  数据结构
    // =================================================================

    private static final class ToolCall {
        String id = "";
        String name = "";
        final StringBuilder args = new StringBuilder();
    }

    private static final class StreamResult {
        final StringBuilder content = new StringBuilder();
        final List<ToolCall> toolCalls = new ArrayList<ToolCall>();
        String finishReason = "";
        boolean sawReasoning = false;
        boolean sawContent = false;

        ToolCall getOrCreate(int idx) {
            while (toolCalls.size() <= idx) toolCalls.add(new ToolCall());
            return toolCalls.get(idx);
        }
    }

    private static final class ApiError extends Exception {
        final int code;
        final String body;
        ApiError(int code, String body) {
            super("HTTP " + code);
            this.code = code;
            this.body = body;
        }
    }
}