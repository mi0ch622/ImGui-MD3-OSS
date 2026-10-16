# 本机构建说明（aarch64 设备 / proot Ubuntu）

本机已在 `/root/imgui_md3`（原生文件系统）成功构建出 `app-debug.apk`。
工作区内的源码已同步为「可构建」状态，改动如下。

## 一、已修复的环境问题

### 1. NDK 工具链 wrapper 丢失 argv[0]（关键）
`/opt/android-sdk/ndk/27.1.12297006/toolchains/llvm/prebuilt/linux-x86_64/bin/`
下的工具被包装成 qemu 脚本，形如：

```bash
exec /usr/bin/qemu-x86_64 -L /usr/x86_64-linux-gnu ".../lld.x86" "$@"
```

LLVM 的 `lld` / `llvm-ar` / `llvm-objcopy` 是**多用途二进制**，靠 `argv[0]` 判断自己扮演哪个工具。
包装脚本把 argv[0] 变成了 `lld.x86`、`llvm-ar.x86`，于是：

* 链接报错 `lld is a generic driver. Invoke ld.lld ...`
* 打包静态库报错 `llvm-ar.x86: error: expected [relpos] for 'a', 'b', or 'i' modifier`

修复：在 `qemu-x86_64` 后加 `-0 <工具名>`，例如

```bash
exec /usr/bin/qemu-x86_64 -L /usr/x86_64-linux-gnu -0 ld.lld ".../lld.x86" "$@"
```

已修 `ld.lld / lld / ld / llvm-ar / llvm-ranlib / llvm-objcopy / llvm-strip / llvm-nm /
llvm-objdump / llvm-readelf`（`.bak` 为备份）。

### 2. FreeType 2.13.2 静态库缺失
原 `CMakeLists.txt` 引用 `cpp/freetype/include` 与 `cpp/freetype/libfreetype.a`，仓库里没有。
已用 NDK 27.1 交叉编译 FreeType 2.13.2（arm64-v8a / android-21 / 关闭 harfbuzz·bzip2·png·brotli·zlib），
产物已放入 `app/src/main/cpp/freetype/`：

```
app/src/main/cpp/freetype/
├── include/            # ft2build.h + freetype/...
└── libfreetype.a       # 7.6 MB, aarch64
```

### 3. libc++ 静态库未自动链接
本环境 clang 驱动在 `-static-libstdc++` 下不会自动补 `-lc++_static`，
导致 `undefined symbol: operator new(unsigned long)` / `__gxx_personality_v0`。
已在 `CMakeLists.txt` 的 `target_link_libraries(imguidemo ...)` 末尾显式追加：

```cmake
c++_static c++abi
```

## 二、构建配置改动

| 文件 | 改动 |
|------|------|
| `app/build.gradle.kts` | `ndkVersion` 改为本机已装的 `27.1.12297006`；移除硬编码 AndroidIDE 工具链参数 |
| `gradle.properties` | `aapt2FromMavenOverride` 指向本机可用包装器 `/opt/android-build/aapt2-bin/aapt2`；`-Xmx` 提到 3072m |
| `settings.gradle.kts` | 阿里云镜像优先（`dl.google.com` 本机不稳） |
| `local.properties` | 新增 `sdk.dir=/opt/android-sdk` |

## 三、构建命令

```bash
cd /root/imgui_md3
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-arm64
/opt/gradle-8.7/bin/gradle --no-daemon assembleDebug
```

* 工具链：JDK 17.0.19 + Gradle 8.7 + AGP 8.0.0 + NDK 27.1.12297006 + CMake 3.22.1
* 产物：`app/build/outputs/apk/debug/app-debug.apk`（14,243,721 B，MD5 `d7503cec5543c689c34ab41b27ddaa9c`）
* 副本：`/storage/emulated/0/Download/Avates/Avates-ImGui-MD3-debug.apk`

## 四、注意事项

* Gradle 必须在原生文件系统（如 `/root`）构建；`/storage/emulated/0` 是 FUSE 挂载，
  存在文件锁与可执行位问题。
* 若换 NDK 版本，`freetype/libfreetype.a` 需用同一版本重编。
* 只编 `arm64-v8a`。
