// 一块带文本标签的全局内存: [TARGET_SCORE_01 (24字节)][int score=1][int watch=0x1234]
// 外部工具端: 在内存里搜 “TARGET_SCORE_01” 这个 ASCII 字符串, 命中地址 +24 就是 score
#include <jni.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

extern "C" {

struct RangeTarget {
    char    tag[24];     // "TARGET_SCORE_01" + 补零
    int32_t score;       // ← 要改的就是它 (初始 1)
    int32_t watch;       // 观察值 0x1234 = 4660 (搜这个数也好定位)
};

__attribute__((used, visibility("default")))
RangeTarget g_rangeTarget = { "TARGET_SCORE_01", 1, 0x1234 };

JNIEXPORT jint JNICALL
Java_com_u3dexample_RangeActivity_nativeGetScore(JNIEnv*, jobject) {
    return g_rangeTarget.score;
}

JNIEXPORT void JNICALL
Java_com_u3dexample_RangeActivity_nativeSetScore(JNIEnv*, jobject, jint v) {
    g_rangeTarget.score = v;
}

// 靶子地址(靶场自己显示出来, 方便对照工具端扫到的地址)
JNIEXPORT jlong JNICALL
Java_com_u3dexample_RangeActivity_nativeScoreAddr(JNIEnv*, jobject) {
    return (jlong)(intptr_t)&g_rangeTarget.score;
}

JNIEXPORT jlong JNICALL
Java_com_u3dexample_RangeActivity_nativeBaseAddr(JNIEnv*, jobject) {
    return (jlong)(intptr_t)&g_rangeTarget;
}

//   作用: 证明靶子确实在本进程里, 并把 模块名 / 标签地址 / score地址 / PID 全打出来
JNIEXPORT jstring JNICALL
Java_com_u3dexample_RangeActivity_nativeSelfScan(JNIEnv* env, jobject) {
    char line[512];
    const char* TAG = "TARGET_SCORE_01";

    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return env->NewStringUTF("打不开 /proc/self/maps");
    FILE* mem = fopen("/proc/self/mem", "rb");
    if (!mem) { fclose(maps); return env->NewStringUTF("打不开 /proc/self/mem"); }

    unsigned long long foundAddr = 0, foundBase = 0, scanned = 0;
    char foundMod[220] = {0};

    while (fgets(line, sizeof(line), maps)) {
        unsigned long a = 0, b = 0;
        char perms[8] = {0}, mod[200] = {0};
        int n = sscanf(line, "%lx-%lx %7s %*s %*s %*s %199s", &a, &b, perms, mod);
        if (n < 3) continue;
        if (perms[0] != 'r' || perms[1] != 'w') continue;      // 只看可写段
        unsigned long len = b - a;
        if (len == 0 || len > (64UL << 20)) continue;

        std::string buf;
        buf.resize(len);
        if (fseek(mem, (long) a, SEEK_SET) != 0) continue;
        size_t got = fread(&buf[0], 1, len, mem);
        if (got == 0) continue;
        scanned += got;

        //    (内存里往往还有同名字符串副本, 不加这层校验会写到副本后面 -> 白改)
        size_t p = buf.find(TAG);
        while (p != std::string::npos) {
            size_t q = p + 28;
            if (q + 2 <= buf.size()
                    && (unsigned char) buf[q] == 0x34
                    && (unsigned char) buf[q + 1] == 0x12) {
                foundAddr = a + (unsigned long long) p;
                foundBase = a;
                if (n >= 4) {
                    size_t i = 0;
                    while (i < sizeof(foundMod) - 1 && mod[i]) { foundMod[i] = mod[i]; i++; }
                }
                break;
            }
            p = buf.find(TAG, p + 1);
        }
        if (foundAddr) break;
    }
    fclose(mem);
    fclose(maps);

    char out[1200];
    if (foundAddr) {
        snprintf(out, sizeof(out),
                 "✅ 自检成功 — 靶子确实在本进程内存里\n"
                 "PID        = %d\n"
                 "标签地址   = 0x%llx\n"
                 "段基址     = 0x%llx\n"
                 "score 地址 = 0x%llx   读回 = %d\n"
                 "所在模块   = %s\n"
                 "已扫可写段 = %llu KB\n"
                 "外部工具请搜 UTF-8 字符串 TARGET_SCORE_01 (24 字节含补零),\n"
                 "命中地址 +24 = score 地址, +28 = watch(=4660)",
                 (int) getpid(), foundAddr, foundBase,
                 (unsigned long long) (uintptr_t) &g_rangeTarget.score,
                 (int) g_rangeTarget.score,
                 foundMod[0] ? foundMod : "(匿名段)",
                 scanned / 1024);
    } else {
        snprintf(out, sizeof(out),
                 "❌ 自检没扫到标签 (已扫 %llu KB 可写段)\nPID = %d",
                 scanned / 1024, (int) getpid());
    }
    return env->NewStringUTF(out);
}

} // extern "C"