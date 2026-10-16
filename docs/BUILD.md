# 构建手册（BUILD）

本文件是 `README.md` 的补充，把「从零到出 APK」的每一步、每条命令都写清楚。
按顺序做即可，不需要 Android Studio 也能完成（全程命令行）。

---

## 0. 一页速查

```bash
# 1) 环境变量
export ANDROID_HOME=$HOME/Android/Sdk
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# 2) 装依赖
yes | sdkmanager --licenses
sdkmanager "platform-tools" "platforms;android-33" "build-tools;33.0.2"
sdkmanager "ndk;27.1.12297006" "cmake;3.22.1"

# 3) 构建
cd ImGui-MD3
echo "sdk.dir=$ANDROID_HOME" > local.properties
./gradlew assembleFullDebug

# 4) 产物
ls app/build/outputs/apk/full/debug/app-full-debug.apk
```

---

## 1. 工具清单与版本

| 工具 | 版本 | 检查命令 |
|---|---|---|
| JDK | 17 | `java -version` |
| Android SDK Platform | android-33 | `sdkmanager --list_installed` |
| Build-Tools | 33.0.2 | 同上 |
| **NDK** | **27.1.12297006** | `ls $ANDROID_HOME/ndk/` |
| CMake | 3.22.1 | `ls $ANDROID_HOME/cmake/` |
| Gradle | 8.1.1（wrapper 自带，无需安装） | `./gradlew -v` |
| AGP | 8.0.0（工程内声明） | — |

> `java -version` 不是 17 时，先设 `JAVA_HOME`，例如：
> ```bash
> export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64   # Ubuntu
> # macOS: export JAVA_HOME=$(/usr/libexec/java_home -v 17)
> ```
> Windows（PowerShell）：`$env:JAVA_HOME="C:\Program Files\Java\jdk-17"`

---

## 2. 安装 SDK 命令行工具

**Windows**

1. 到 Android 官网下载 `commandlinetools-win-*.zip`
2. 解压到 `C:\Android\cmdline-tools\latest\`（注意必须是 `latest` 这一层，里面有 `bin/`）
3. 环境变量：`ANDROID_HOME=C:\Android`，`PATH` 里加 `%ANDROID_HOME%\cmdline-tools\latest\bin`

**macOS / Linux**

```bash
mkdir -p $HOME/Android/Sdk/cmdline-tools
cd /tmp
# 从 Android 官网下载最新 commandlinetools-<平台>-*.zip，然后：
unzip commandlinetools-linux-*.zip -d $HOME/Android/Sdk/cmdline-tools
mv $HOME/Android/Sdk/cmdline-tools/cmdline-tools $HOME/Android/Sdk/cmdline-tools/latest

export ANDROID_HOME=$HOME/Android/Sdk
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH
```

验证：`sdkmanager --version`

---

## 3. 安装 SDK / NDK / CMake

```bash
yes | sdkmanager --licenses

sdkmanager "platform-tools"
sdkmanager "platforms;android-33"
sdkmanager "build-tools;33.0.2"
sdkmanager "ndk;27.1.12297006"
sdkmanager "cmake;3.22.1"

sdkmanager --list_installed | grep -E "ndk|cmake|platforms|build-tools"
```

> **国内网络**：
> `sdkmanager` 默认走 `https://dl.google.com`，慢的话给它挂代理（`export https_proxy=...`），
> 或者用国内镜像源（把 `ANDROID_SDK_ROOT` 的仓库 XML 指向镜像）。
>
> **想换 NDK 版本**：改 `app/build.gradle.kts` 里的 `ndkVersion = "x.y.z"`，
> 然后 `sdkmanager "ndk;x.y.z"` 装上同版本，两边必须一致。

---

## 4. 构建

```bash
git clone <你的仓库地址> ImGui-MD3
cd ImGui-MD3
echo "sdk.dir=$ANDROID_HOME" > local.properties
```

### 4.1 常用命令

```bash
./gradlew assembleFullDebug        # 只打主 App（MD3 界面）
./gradlew assembleDebug            # 打全部风味
./gradlew assembleFullRelease      # Release（需要自己配签名）
./gradlew clean                    # 清理
./gradlew --stop                   # 停掉守护进程（改配置后建议执行）
```

Windows 用 `gradlew.bat` 替换 `./gradlew`。

### 4.2 产物路径

```
app/build/outputs/apk/full/debug/app-full-debug.apk
app/build/outputs/apk/proctest/debug/app-proctest-debug.apk
app/build/outputs/apk/range/debug/app-range-debug.apk
```

### 4.3 安装到手机

```bash
adb install -r app/build/outputs/apk/full/debug/app-full-debug.apk
```

系统弹「是否安装」要允许；国产 ROM（小米/华为等）可能还要开启「USB 安装」开关。

### 4.4 编译产物太大想瘦身

- `app/src/main/cpp/live2d` + `cubism`（约 37MB 源码）只在用立绘时才需要，
  不用的话把 `CMakeLists.txt` 里对应的 `add_library(cubism ...)`、`add_library(live2dapp ...)`
  以及 `target_link_libraries(imguidemo ... cubism live2dapp ...)` 里的引用删掉，再把两个目录删除。
- `imgui-1.91.8/examples` 是上游示例，不参与编译，可以直接删。

---

## 5. 在 ARM64 环境构建（手机 / 开发板 / 树莓派）

x86_64 电脑上不用看这节。在 ARM64 上构建会遇到两个坑：

### 5.1 aapt2 架构不匹配

Google 官方仓库里的 `aapt2` 只提供 x86_64 二进制，ARM64 上会报
`aapt2: syntax error / cannot execute binary file`。

解决办法：

1. 自己编译或获取 **arm64 版 aapt2**（来源：`android-<版本>-aapt2-sources` 或第三方为 Termux 构建的版本）；
2. 在 `gradle.properties` 里放开（把注释去掉，指向你的 aapt2 可执行文件）：
   ```properties
   android.aapt2FromMavenOverride=/你的/aapt2/绝对路径
   ```

### 5.2 不要在 FUSE 挂载目录里构建

`/storage/emulated/0/...`（手机内部存储）是 FUSE 挂载，Gradle 的文件锁、
CMake 的 `exec` 都会出问题（表现为卡死、`Text file busy`、`Invalid argument`）。

**把工程复制到原生文件系统再构建**，例如：

```bash
cp -r /storage/emulated/0/ImGui-MD3 /root/imgui_md3
cd /root/imgui_md3
./gradlew assembleFullDebug
```

### 5.3 NDK 里的 clang 也是 x86_64 怎么办

用 `qemu-user-static` 包装即可（已在 aarch64 设备上验证可以完整编译本工程）：

```bash
apt install qemu-user-static
# 然后把 NDK 的 clang / clang++ / lld 等二进制用 qemu-aarch64-static 包一层，
# 或直接使用为 arm64 提供的 NDK（如 Termux 的 ndk 包）
```

---

## 6. 改配置速查

| 想改什么 | 改哪里 |
|---|---|
| 包名 | `app/build.gradle.kts` → `defaultConfig.applicationId` 和各 `productFlavors` |
| 应用名 | `app/src/main/res/values/strings.xml`（风味可单独覆盖） |
| 图标 | `app/src/main/res/mipmap-*/ic_launcher.png` |
| 版本号 | `versionCode` / `versionName` |
| NDK 版本 | `ndkVersion` + `sdkmanager "ndk;新版本"` |
| CMake 参数（如开/关 FreeType） | `app/build.gradle.kts` → `externalNativeBuild.cmake.arguments` |
| 主题种子色 | `app/src/main/cpp/md3_theme.cpp` |
| 字体 | `app/src/main/assets/` + 字体加载处 |
| 多语言文案 | `app/src/main/cpp/md3_i18n.cpp` |
| AI 密钥 / 接口地址 / 模型名 | `app/src/main/java/com/u3dexample/AiChatBridge.java` |

---

## 7. 调试

```bash
# 看原生日志（C++ 侧用 __android_log_print 打的都在这）
adb logcat | grep -iE "imguidemo|md3"

# 看崩溃堆栈
adb logcat -b crash

# 看 CMake 配置过程（构建失败时）
./gradlew assembleFullDebug --info | grep -i cmake
```

`app/src/main/cpp/md3_log.cpp` 里封装了日志输出，C++ 侧可直接调用。

---

## 8. 排错对照表

| 报错 | 处理 |
|---|---|
| `No version of NDK matched the requested version` | 装与 `ndkVersion` 匹配的 NDK，或改 `ndkVersion` |
| `Failed to find CMake` / `CMake '3.22.1' was not found` | `sdkmanager "cmake;3.22.1"` |
| `AAPT2 aapt2-*-linux.jar` 相关错误 | 见 5.1（ARM64 aapt2） |
| `Could not find com.android.tools.build:gradle:8.0.0` | 网络问题，挂代理或换仓库镜像 |
| 构建卡在 `> Task :app:externalNativeBuildDebug` | 多半在 FUSE 目录，见 5.2 |
| `Text file busy` | 同上；或 `./gradlew --stop` 后重试 |
| 运行时闪退、日志 `UnsatisfiedLinkError` | `.so` 没打进 APK：检查 `CMakeLists.txt` 里是否把对应 cpp 加进 `add_library(imguidemo ...)` |
| 中文显示 `???` | 字体没加载成功：确认 `assets` 里的字体路径与代码一致 |
| 触摸位置偏 | 触摸必须挂在 `GLSurfaceView.setOnTouchListener`，不要用 `Activity.onTouchEvent` |

---

## 9. 打包成 zip 分发（可选）

```bash
# 排除构建目录后再压缩
zip -r ImGui-MD3.zip . -x "*/build/*" "*.git/*" "*.gradle/*" "*local.properties"
```

Windows 上用资源管理器右键压缩也可以，注意别把 `build/`、`.gradle/`、`.idea/` 打进去。
