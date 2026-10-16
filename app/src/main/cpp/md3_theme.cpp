// md3_theme.cpp - MD3 color palettes and theme setup
#include "md3_common.h"

const Md3Palette kPalettes[THEME_COUNT][2] = {
    {
        {
            Md3(166,201,255), Md3(0,50,110), Md3(0,74,139), Md3(226,236,255),
            Md3(177,189,210), Md3(56,69,92),
            Md3(12,14,22), Md3(24,27,36), Md3(35,38,50), Md3(36,39,50),
            Md3(224,226,234), Md3(193,200,216),
            Md3(138,145,162), Md3(68,75,91),
            Md3(206,191,255), Md3(80,65,139),
        },
        {
            Md3(0,95,180), Md3(255,255,255), Md3(219,228,255), Md3(0,30,62),
            Md3(86,98,119), Md3(218,229,252),
            Md3(251,248,255), Md3(240,237,245), Md3(232,228,238), Md3(224,224,236),
            Md3(27,29,37), Md3(63,66,80),
            Md3(112,116,131), Md3(190,193,208),
            Md3(107,84,155), Md3(240,219,255),
        },
    },
    {
        {
            Md3(141,224,172), Md3(0,70,29), Md3(0,92,45), Md3(215,255,221),
            Md3(116,182,150), Md3(30,88,64),
            Md3(15,18,16), Md3(27,31,27), Md3(38,43,38), Md3(38,43,40),
            Md3(219,226,220), Md3(188,212,200),
            Md3(132,156,143), Md3(58,83,69),
            Md3(182,207,157), Md3(56,80,56),
        },
        {
            Md3(0,101,48), Md3(255,255,255), Md3(199,255,217), Md3(0,31,14),
            Md3(72,126,97), Md3(200,254,223),
            Md3(252,253,247), Md3(241,247,238), Md3(234,239,232), Md3(227,236,228),
            Md3(24,29,26), Md3(60,72,64),
            Md3(106,122,111), Md3(188,202,193),
            Md3(102,128,79), Md3(234,255,233),
        },
    },
    {
        {
            Md3(255,210,173), Md3(105,47,0), Md3(141,74,11), Md3(255,229,211),
            Md3(220,181,151), Md3(84,53,27),
            Md3(21,18,16), Md3(33,30,27), Md3(45,41,38), Md3(42,39,37),
            Md3(231,222,217), Md3(210,195,184),
            Md3(154,140,130), Md3(76,63,55),
            Md3(255,207,180), Md3(123,73,63),
        },
        {
            Md3(174,84,14), Md3(255,255,255), Md3(255,221,204), Md3(64,26,0),
            Md3(133,89,58), Md3(255,221,195),
            Md3(255,251,247), Md3(251,239,230), Md3(245,224,213), Md3(246,231,222),
            Md3(28,25,23), Md3(75,59,48),
            Md3(125,104,90), Md3(213,192,179),
            Md3(142,96,77), Md3(255,221,207),
        },
    },
    {
        {
            Md3(255,197,202), Md3(120,18,28), Md3(155,24,39), Md3(255,219,223),
            Md3(221,172,177), Md3(91,42,48),
            Md3(20,17,18), Md3(32,28,30), Md3(44,40,42), Md3(40,37,39),
            Md3(234,222,224), Md3(214,193,196),
            Md3(159,139,142), Md3(75,55,59),
            Md3(255,196,174), Md3(136,58,38),
        },
        {
            Md3(153,12,28), Md3(255,255,255), Md3(255,217,219), Md3(85,0,15),
            Md3(118,69,75), Md3(255,217,221),
            Md3(255,251,252), Md3(250,237,239), Md3(244,222,224), Md3(248,229,231),
            Md3(27,24,25), Md3(77,56,60),
            Md3(130,106,110), Md3(216,192,195),
            Md3(137,71,52), Md3(255,217,200),
        },
    },
    {
        {
            Md3(208,188,255), Md3(56,30,114), Md3(79,55,139), Md3(234,221,255),
            Md3(204,194,220), Md3(74,68,88),
            Md3(20,18,24), Md3(33,32,37), Md3(43,41,48), Md3(41,39,46),
            Md3(230,224,233), Md3(202,196,208),
            Md3(147,143,153), Md3(73,69,79),
            Md3(239,184,200), Md3(99,59,72),
        },
        {
            Md3(107,67,200), Md3(255,255,255), Md3(234,221,255), Md3(56,30,114),
            Md3(98,91,113), Md3(234,222,248),
            Md3(252,248,253), Md3(244,239,244), Md3(236,230,236), Md3(231,224,236),
            Md3(28,27,31), Md3(67,68,73),
            Md3(116,116,126), Md3(200,198,208),
            Md3(125,82,96), Md3(255,216,228),
        },
    },
};

int g_themeScheme = SCHEME_BLUE;
bool g_darkMode = true;

ImVec4 Md3(float r, float g, float b, float a) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

const Md3Palette& CurPalette() {
    return kPalettes[g_themeScheme][g_darkMode ? 0 : 1];
}

ImU32 Md3U32(const ImVec4& c) { return ImGui::GetColorU32(c); }

void SetupMD3Theme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 28.0f;
    s.ChildRounding     = 16.0f;
    s.FrameRounding     = 20.0f;
    s.PopupRounding     = 16.0f;
    s.ScrollbarRounding = 12.0f;
    s.GrabRounding      = 10.0f;
    s.TabRounding       = 12.0f;

    s.WindowPadding    = ImVec2(16, 14);
    s.FramePadding     = ImVec2(14, 10);
    s.ItemSpacing      = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.IndentSpacing    = 16.0f;
    s.ScrollbarSize    = 18.0f;   // 恢复原值（不要动）
    s.GrabMinSize      = 20.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 0.0f;
    s.PopupBorderSize  = 1.0f;
    s.FrameBorderSize  = 0.0f;

    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    const ImVec4& Primary            = CurPalette().Primary;
    const ImVec4& OnPrimary          = CurPalette().OnPrimary;
    const ImVec4& PrimaryContainer   = CurPalette().PrimaryContainer;
    const ImVec4& OnPrimaryContainer = CurPalette().OnPrimaryContainer;
    const ImVec4& Secondary          = CurPalette().Secondary;
    const ImVec4& SecondaryContainer = CurPalette().SecondaryContainer;
    const ImVec4& Surface            = CurPalette().Surface;
    const ImVec4& SurfaceContainer   = CurPalette().SurfaceContainer;
    const ImVec4& SurfaceContainerHigh = CurPalette().SurfaceContainerHigh;
    const ImVec4& SurfaceVariant     = CurPalette().SurfaceVariant;
    const ImVec4& OnSurface          = CurPalette().OnSurface;
    const ImVec4& OnSurfaceVariant   = CurPalette().OnSurfaceVariant;
    const ImVec4& Outline            = CurPalette().Outline;
    const ImVec4& OutlineVariant     = CurPalette().OutlineVariant;
    const ImVec4& Tertiary           = CurPalette().Tertiary;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                   = OnSurface;
    c[ImGuiCol_TextDisabled]           = Outline;
    c[ImGuiCol_WindowBg]               = SurfaceContainer;
    c[ImGuiCol_ChildBg]                = SurfaceContainerHigh;
    c[ImGuiCol_PopupBg]                = SurfaceContainerHigh;
    c[ImGuiCol_Border]                 = OutlineVariant;
    c[ImGuiCol_BorderShadow]           = Md3(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]                = SurfaceContainerHigh;
    c[ImGuiCol_FrameBgHovered]         = SurfaceVariant;
    c[ImGuiCol_FrameBgActive]          = PrimaryContainer;
    c[ImGuiCol_TitleBg]                = SurfaceContainer;
    c[ImGuiCol_TitleBgActive]          = SurfaceContainerHigh;
    c[ImGuiCol_TitleBgCollapsed]       = SurfaceContainer;
    c[ImGuiCol_Button]                 = Primary;
    c[ImGuiCol_ButtonHovered]          = Md3(Primary.x * 255 + 12, Primary.y * 255 + 12, Primary.z * 255 + 12);
    c[ImGuiCol_ButtonActive]           = Md3(Primary.x * 255 - 18, Primary.y * 255 - 18, Primary.z * 255 - 18);
    c[ImGuiCol_Header]                 = SecondaryContainer;
    c[ImGuiCol_HeaderHovered]          = SurfaceVariant;
    c[ImGuiCol_HeaderActive]           = PrimaryContainer;
    c[ImGuiCol_SliderGrab]             = Primary;
    c[ImGuiCol_SliderGrabActive]       = Md3(Primary.x * 255 + 12, Primary.y * 255 + 12, Primary.z * 255 + 12);
    c[ImGuiCol_CheckMark]              = Primary;
    c[ImGuiCol_Separator]              = OutlineVariant;
    c[ImGuiCol_SeparatorHovered]       = Primary;
    c[ImGuiCol_SeparatorActive]        = Primary;
    c[ImGuiCol_ResizeGrip]             = OutlineVariant;
    c[ImGuiCol_ResizeGripHovered]      = Primary;
    c[ImGuiCol_ResizeGripActive]       = Primary;
    c[ImGuiCol_Tab]                    = SurfaceContainerHigh;
    c[ImGuiCol_TabHovered]             = SurfaceVariant;
    c[ImGuiCol_TabActive]              = SecondaryContainer;
    c[ImGuiCol_TabUnfocused]           = SurfaceContainer;
    c[ImGuiCol_TabUnfocusedActive]     = SurfaceContainerHigh;
    c[ImGuiCol_ScrollbarBg]            = Md3(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]          = OutlineVariant;
    c[ImGuiCol_ScrollbarGrabHovered]   = Outline;
    c[ImGuiCol_ScrollbarGrabActive]    = Primary;
    c[ImGuiCol_TextSelectedBg]         = Md3(PrimaryContainer.x * 255, PrimaryContainer.y * 255, PrimaryContainer.z * 255, 0.5f);
    c[ImGuiCol_NavHighlight]           = Primary;
    c[ImGuiCol_NavWindowingHighlight]  = Primary;
    c[ImGuiCol_NavWindowingDimBg]      = Md3(0, 0, 0, 0.4f);
    c[ImGuiCol_ModalWindowDimBg]       = Md3(0, 0, 0, 0.55f);
    c[ImGuiCol_DragDropTarget]         = Tertiary;
}
