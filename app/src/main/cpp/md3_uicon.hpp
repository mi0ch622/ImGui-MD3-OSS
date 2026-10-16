// md3_uicon.hpp - UI 浮动圆角图标(纹理)
#pragma once

namespace UiIcon {
unsigned int Texture();      // 0 = 未就绪
void EnsureTexture();        // 懒加载 (需 GL 上下文中调用)
}
