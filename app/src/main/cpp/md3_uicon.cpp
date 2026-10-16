// md3_uicon.cpp - UI 隐藏时的浮动圆角图标纹理
#include "md3_common.h"
#include <GLES3/gl3.h>
#include "live2d/stb_image.h"
#include "md3_uicon.hpp"
#include "md3_uicon_data.h"

namespace UiIcon {
static unsigned int s_tex = 0;
static bool s_tried = false;

unsigned int Texture() { return s_tex; }

void EnsureTexture() {
    if (s_tex != 0 || s_tried) return;
    s_tried = true;
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(kUiIconPng, (int)kUiIconPngSize, &w, &h, &comp, 4);
    if (px == nullptr || w <= 0 || h <= 0) return;
    GLint prev = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev);
    s_tex = (unsigned int)t;
}

} // namespace UiIcon
