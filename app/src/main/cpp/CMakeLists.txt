#
# Created for Xouer.
#      2026.08.05
#

cmake_minimum_required(VERSION 3.22)
project("u3dexample" CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ANDROID_STL c++_static CACHE STRING "" FORCE)

# imgui 源码 (v1.91.8)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.91.8)

# FreeType 路径 (修改为你本地的 FreeType 路径)
set(FREETYPE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include)
set(FREETYPE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/freetype/libfreetype.a)


# imgui 核心 + Android / OpenGL3 后端 + FreeType 后端
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_android.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/freetype
    ${FREETYPE_INCLUDE_DIR}
)

# =====================================================================
# Live2D Cubism SDK (方案照 ImGuiLive2d-main: 模型数据编译进 so, 无需 assets)
#   live2d/Framework  = Cubism Native Framework 全量源码
#   live2d/Core       = Cubism Core 头文件 + arm64 静态库
#   live2d/Model/Live2DModelData_Hiyori.h = Hiyori 模型(内嵌 C 数组)
# =====================================================================
set(LIVE2D_DIR ${CMAKE_CURRENT_SOURCE_DIR}/live2d)
set(CUBISM_DIR ${LIVE2D_DIR}/Framework)

file(GLOB_RECURSE CUBISM_SRC ${CUBISM_DIR}/*.cpp)
# 只保留平台无关 + OpenGL ES2 部分 (剔掉 D3D9/D3D11/Metal/Vulkan, Android 用不到也编不过)
list(FILTER CUBISM_SRC EXCLUDE REGEX "/(D3D9|D3D11|Metal|Vulkan)/")

add_library(cubism STATIC ${CUBISM_SRC})
target_include_directories(cubism PUBLIC
    ${CUBISM_DIR}
    ${LIVE2D_DIR}/Core/include
)
target_compile_definitions(cubism PUBLIC CSM_TARGET_ANDROID_ES2)
target_link_libraries(cubism PUBLIC
    ${LIVE2D_DIR}/Core/lib/android/arm64-v8a/libLive2DCubismCore.a
    GLESv2 log
)

# 应用层: 分配器/平台/纹理加载/模型管理/模型 (内嵌数据加载器在这层)
add_library(live2dapp STATIC
    ${LIVE2D_DIR}/LAppAllocator.cpp
    ${LIVE2D_DIR}/LAppDefine.cpp
    ${LIVE2D_DIR}/LAppPal.cpp
    ${LIVE2D_DIR}/TextureLoader.cpp
    ${LIVE2D_DIR}/stb_image_impl.cpp
    ${LIVE2D_DIR}/Live2DManager.cpp
    ${LIVE2D_DIR}/Live2DModel.cpp
)
target_include_directories(live2dapp PUBLIC ${LIVE2D_DIR})
target_link_libraries(live2dapp PUBLIC cubism GLESv2 log)

# JNI 演示层 (拆分为多个 MD3 模块)
add_library(imguidemo SHARED
    md3_jni.cpp
    proctest.cpp
    rangevalue.cpp
    md3_live2d.cpp
    md3_tts.cpp
    md3_fx.cpp
    md3_theme.cpp
    md3_i18n.cpp
    md3_icons.cpp
    md3_anim.cpp
    md3_controls.cpp
    md3_log.cpp
    md3_uianim.cpp
    md3_motionblur.cpp
    md3_uicon.cpp
    md3_pages.cpp
    md3_ai.cpp
    md3_net.cpp
    md3_mcp.cpp
    md3_common.h
)
target_link_libraries(imguidemo
    imgui
    live2dapp                                   # Live2D 应用层(自动带上 cubism + Core 静态库)
    ${FREETYPE_LIB} -lz
    android EGL GLESv2 log
    # 显式链接 libc++ 静态库: 本环境 NDK clang 驱动不会因 -static-libstdc++ 自动补 -lc++_static
    c++_static c++abi
)

# Android 上 imgui opengl3 后端必须指定 GLES2 加载器模式
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES2 IMGUI_ENABLE_FREETYPE)
