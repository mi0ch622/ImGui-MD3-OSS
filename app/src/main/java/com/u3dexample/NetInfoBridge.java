package com.u3dexample;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.URL;
import java.util.Enumeration;

/**
 * 「网络与位置」探测桥接层。
 *
 * 由 C++ 侧 PageNetCard() 调用 {@link #refresh()}，后台线程完成：
 *   1. VPN 状态：ConnectivityManager 的 TRANSPORT_VPN + tun/ppp/wg 网卡兜底
 *   2. 内网 IP：遍历网卡取第一个非回环 IPv4
 *   3. 公网 IP + 详细归属地：ipwho.is 主源、ipinfo.io 备源
 * 结果整理成固定字段的 JSON 回调给 native 渲染。
 */
public final class NetInfoBridge {

    private static volatile Context  sCtx;
    private static volatile boolean  sRunning;

    public static native void nativeOnNetInfo(String json);

    private NetInfoBridge() {}

    public static void attach(Context c) {
        if (c != null) sCtx = c.getApplicationContext();
    }

    public static boolean isRunning() { return sRunning; }

    // =================================================================
    //  入口
    // =================================================================
    public static void refresh() {
        if (sRunning) return;
        sRunning = true;

        Thread t = new Thread(new Runnable() {
            @Override public void run() {
                JSONObject o = new JSONObject();
                try {
                    o.put("vpn", isVpnActive() ? 1 : 0);
                    o.put("localIp", localIp());
                } catch (Throwable ignored) { }

                String[][] sources = new String[][] {
                        { "https://ipwho.is/",       "ipwho"  },
                        { "https://ipinfo.io/json",  "ipinfo" },
                };

                boolean got = false;
                for (int i = 0; i < sources.length && !got; i++) {
                    try {
                        String body = httpGet(sources[i][0]);
                        if (body == null || body.length() == 0) continue;
                        if (parse(o, body, sources[i][1])) {
                            o.put("source", sources[i][1]);
                            got = true;
                        }
                    } catch (Throwable ignored) { }
                }

                if (!got) {
                    try { o.put("error", "归属地查询失败（网络不可达）"); } catch (Throwable ignored) { }
                }
                try { o.put("ok", 1); } catch (Throwable ignored) { }

                nativeOnNetInfo(o.toString());
                sRunning = false;
            }
        }, "net-info");
        t.start();
    }

    // =================================================================
    //  VPN / 内网 IP
    // =================================================================
    private static boolean isVpnActive() {
        Context ctx = sCtx;
        if (ctx != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                ConnectivityManager cm = (ConnectivityManager)
                        ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
                if (cm != null) {
                    for (Network n : cm.getAllNetworks()) {
                        NetworkCapabilities nc = cm.getNetworkCapabilities(n);
                        if (nc != null && nc.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
                            return true;
                        }
                    }
                }
            } catch (Throwable ignored) { }
        }
        // 兜底：VPN 会创建 tun/ppp/wg 这类虚拟网卡
        try {
            Enumeration<NetworkInterface> ifs = NetworkInterface.getNetworkInterfaces();
            while (ifs != null && ifs.hasMoreElements()) {
                NetworkInterface ni = ifs.nextElement();
                String n = ni.getName();
                if (n == null) continue;
                if (n.startsWith("tun") || n.startsWith("ppp")
                        || n.startsWith("wg") || n.startsWith("utun")) {
                    boolean up = false;
                    try { up = ni.isUp(); } catch (Throwable ignored) { }
                    if (up) return true;
                }
            }
        } catch (Throwable ignored) { }
        return false;
    }

    private static String localIp() {
        try {
            Enumeration<NetworkInterface> ifs = NetworkInterface.getNetworkInterfaces();
            while (ifs != null && ifs.hasMoreElements()) {
                NetworkInterface ni = ifs.nextElement();
                if (!ni.isUp() || ni.isLoopback()) continue;
                String n = ni.getName();
                if (n == null) continue;
                if (n.startsWith("tun") || n.startsWith("ppp")
                        || n.startsWith("wg") || n.startsWith("utun")) continue;
                for (java.net.InterfaceAddress ia : ni.getInterfaceAddresses()) {
                    InetAddress a = ia.getAddress();
                    if (a instanceof Inet4Address && !a.isLoopbackAddress()) {
                        return a.getHostAddress();
                    }
                }
            }
        } catch (Throwable ignored) { }
        return "";
    }

    // =================================================================
    //  解析
    // =================================================================
    private static boolean parse(JSONObject o, String body, String kind) {
        try {
            JSONObject j = new JSONObject(body);
            String ip = j.optString("ip", "");
            if (ip.length() == 0) return false;
            o.put("publicIp", ip);

            if ("ipwho".equals(kind)) {
                String cc = j.optString("country_code", "");
                o.put("country", cnName(cc, j.optString("country", "")));
                o.put("region",  cnRegion(j.optString("region", "")));
                o.put("city",    cnCity(j.optString("city", "")));
                o.put("lat",     j.optString("latitude", ""));
                o.put("lon",     j.optString("longitude", ""));
                o.put("postal",  j.optString("postal", ""));
                JSONObject conn = j.optJSONObject("connection");
                if (conn != null) {
                    // ipwho.is 的 isp 字段其实是机房地址，org 才是运营商名
                    String org = conn.optString("org", "");
                    String isp = conn.optString("isp", "");
                    if (org.length() == 0) org = isp;
                    if (org.startsWith("No.") && isp.length() > 0 && !isp.startsWith("No.")) {
                        org = isp;
                    }
                    String asn = conn.optString("asn", "");
                    o.put("isp", asn.length() > 0 ? (org + "  AS" + asn) : org);
                }
                JSONObject tz = j.optJSONObject("timezone");
                if (tz != null) {
                    String id = tz.optString("id", "");
                    String utc = tz.optString("utc", "");
                    o.put("tz", utc.length() > 0 ? (id + "  UTC" + utc) : id);
                }
            } else {
                String cc = j.optString("country", "");
                o.put("country", cnName(cc, cc));
                o.put("region",  cnRegion(j.optString("region", "")));
                o.put("city",    cnCity(j.optString("city", "")));
                String loc = j.optString("loc", "");
                if (loc.contains(",")) {
                    String[] parts = loc.split(",");
                    o.put("lat", parts[0]);
                    o.put("lon", parts.length > 1 ? parts[1] : "");
                }
                o.put("postal", j.optString("postal", ""));
                o.put("isp",    j.optString("org", ""));
                o.put("tz",     j.optString("timezone", ""));
            }
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    private static String cnName(String cc, String fallback) {
        if (cc == null) return fallback == null ? "" : fallback;
        String c = cc.trim().toUpperCase();
        String[][] map = {
                { "CN", "中国" },      { "HK", "中国香港" }, { "MO", "中国澳门" },
                { "TW", "中国台湾" },  { "US", "美国" },     { "JP", "日本" },
                { "KR", "韩国" },      { "SG", "新加坡" },   { "MY", "马来西亚" },
                { "TH", "泰国" },      { "VN", "越南" },     { "PH", "菲律宾" },
                { "ID", "印度尼西亚" },{ "IN", "印度" },     { "GB", "英国" },
                { "UK", "英国" },      { "DE", "德国" },     { "FR", "法国" },
                { "RU", "俄罗斯" },    { "CA", "加拿大" },   { "AU", "澳大利亚" },
                { "NL", "荷兰" },      { "BR", "巴西" },     { "AE", "阿联酋" },
        };
        for (int i = 0; i < map.length; i++) {
            if (map[i][0].equals(c)) return map[i][1];
        }
        return (fallback == null || fallback.length() == 0) ? c : fallback;
    }

    /** 省份拼音 -> 中文（ipwho.is / ipinfo 都只给拼音） */
    private static String cnRegion(String region) {
        if (region == null) return "";
        String r = region.trim();
        if (r.length() == 0) return "";
        String[][] map = {
                { "Shaanxi", "陕西省" },   { "Shanxi", "山西省" },   { "Beijing", "北京市" },
                { "Tianjin", "天津市" },   { "Shanghai", "上海市" }, { "Chongqing", "重庆市" },
                { "Hebei", "河北省" },     { "Liaoning", "辽宁省" }, { "Jilin", "吉林省" },
                { "Heilongjiang", "黑龙江省" }, { "Jiangsu", "江苏省" }, { "Zhejiang", "浙江省" },
                { "Anhui", "安徽省" },     { "Fujian", "福建省" },   { "Jiangxi", "江西省" },
                { "Shandong", "山东省" },  { "Henan", "河南省" },    { "Hubei", "湖北省" },
                { "Hunan", "湖南省" },     { "Guangdong", "广东省" }, { "Guangxi", "广西壮族自治区" },
                { "Hainan", "海南省" },    { "Sichuan", "四川省" },  { "Guizhou", "贵州省" },
                { "Yunnan", "云南省" },    { "Xizang", "西藏自治区" }, { "Tibet", "西藏自治区" },
                { "Gansu", "甘肃省" },     { "Qinghai", "青海省" },  { "Ningxia", "宁夏回族自治区" },
                { "Xinjiang", "新疆维吾尔自治区" }, { "Mongol", "内蒙古自治区" },
                { "Taiwan", "台湾省" },    { "Xianggang", "香港特别行政区" }, { "Hong Kong", "香港特别行政区" },
                { "Aomen", "澳门特别行政区" }, { "Macau", "澳门特别行政区" },
        };
        String lower = r.toLowerCase();
        for (int i = 0; i < map.length; i++) {
            if (lower.contains(map[i][0].toLowerCase())) return map[i][1];
        }
        return r;
    }

    /** 主要城市拼音 -> 中文 */
    private static String cnCity(String city) {
        if (city == null) return "";
        String c = city.trim();
        if (c.length() == 0) return "";
        String[][] map = {
                { "Wuhan", "武汉市" },      { "Beijing", "北京市" },   { "Shanghai", "上海市" },
                { "Guangzhou", "广州市" },  { "Shenzhen", "深圳市" },  { "Hangzhou", "杭州市" },
                { "Chengdu", "成都市" },    { "Nanjing", "南京市" },   { "Tianjin", "天津市" },
                { "Chongqing", "重庆市" },  { "Xi'an", "西安市" },     { "Xian", "西安市" },
                { "Suzhou", "苏州市" },     { "Zhengzhou", "郑州市" }, { "Changsha", "长沙市" },
                { "Qingdao", "青岛市" },    { "Dalian", "大连市" },    { "Xiamen", "厦门市" },
                { "Fuzhou", "福州市" },     { "Hefei", "合肥市" },     { "Kunming", "昆明市" },
                { "Nanchang", "南昌市" },   { "Nanning", "南宁市" },   { "Guiyang", "贵阳市" },
                { "Harbin", "哈尔滨市" },   { "Shenyang", "沈阳市" },  { "Changchun", "长春市" },
                { "Shijiazhuang", "石家庄市" }, { "Taiyuan", "太原市" }, { "Lanzhou", "兰州市" },
                { "Urumqi", "乌鲁木齐市" }, { "Haikou", "海口市" },    { "Sanya", "三亚市" },
                { "Ningbo", "宁波市" },     { "Wuxi", "无锡市" },      { "Foshan", "佛山市" },
                { "Dongguan", "东莞市" },   { "Zhuhai", "珠海市" },    { "Wenzhou", "温州市" },
                { "Jinan", "济南市" },      { "Yantai", "烟台市" },    { "Luoyang", "洛阳市" },
                { "Hong Kong", "香港" },    { "Macau", "澳门" },       { "Taipei", "台北市" },
        };
        for (int i = 0; i < map.length; i++) {
            if (map[i][0].equalsIgnoreCase(c)) return map[i][1];
        }
        return c;
    }

    // =================================================================
    //  HTTP
    // =================================================================
    private static String httpGet(String url) throws Exception {
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setRequestProperty("User-Agent",
                "Mozilla/5.0 (Linux; Android 13; Mobile) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36");
        conn.setRequestProperty("Accept", "application/json,text/plain,*/*");
        conn.setConnectTimeout(12000);
        conn.setReadTimeout(15000);
        conn.setInstanceFollowRedirects(true);
        int code = conn.getResponseCode();
        InputStream in = (code >= 200 && code < 400) ? conn.getInputStream() : conn.getErrorStream();
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        if (in != null) {
            byte[] buf = new byte[8192];
            int n, total = 0;
            while ((n = in.read(buf)) > 0) {
                bos.write(buf, 0, n);
                total += n;
                if (total > 128 * 1024) break;
            }
            try { in.close(); } catch (Throwable ignored) { }
        }
        try { conn.disconnect(); } catch (Throwable ignored) { }
        try { return bos.toString("UTF-8"); } catch (Throwable e) { return ""; }
    }
}