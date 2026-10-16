// =====================================================================
// stb_image_impl.cpp - stb_image 的单一定义单元
// ---------------------------------------------------------------------
// live2d/TextureLoader.cpp 只做 "声明式" 包含 stb_image.h
// (它那行 #define STB_IMAGE_IMPLEMENTATION 是注释掉的, 实现放在这里),
// 所以整个工程里必须恰好有一个 TU 打开实现宏, 否则链接期缺
// stbi_load_from_memory / stbi_failure_reason / stbi_image_free。
// =====================================================================
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO          // Android 上只用内存解码, 不要 fopen 那套
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"