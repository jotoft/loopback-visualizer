# ImGui Styling & Theming Guide

## The Problem with Default ImGui

ImGui's default style was designed by programmers, for programmers. It's functional but:
- Looks dated (Windows 95 vibes)
- Poor contrast and readability
- Inconsistent spacing
- No visual hierarchy
- Harsh colors

This guide will transform ImGui into a modern, beautiful UI.

## Understanding ImGui Style System

### Style Components

```cpp
ImGuiStyle& style = ImGui::GetStyle();

// Sizing & Spacing
style.WindowPadding      // Padding within windows
style.FramePadding       // Padding within frames (buttons, inputs)
style.ItemSpacing        // Spacing between items
style.ItemInnerSpacing   // Spacing within items
style.IndentSpacing      // Indentation for trees
style.ScrollbarSize      // Width of scrollbars
style.GrabMinSize        // Minimum size of grab handles

// Borders & Rounding  
style.WindowBorderSize   // Window border thickness
style.FrameBorderSize    // Frame border thickness
style.WindowRounding     // Window corner radius
style.FrameRounding      // Button/input corner radius
style.GrabRounding       // Slider grab rounding

// Colors (39 elements)
style.Colors[ImGuiCol_WindowBg]         // Window background
style.Colors[ImGuiCol_Text]             // Default text
style.Colors[ImGuiCol_Button]           // Button color
// ... and many more
```

## Modern Dark Theme

```cpp
void applyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Sizing
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 16.0f;
    style.GrabMinSize       = 8.0f;
    
    // Borders
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    
    // Rounding
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    
    // Alignment
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
    
    // Colors
    ImVec4* colors = style.Colors;
    
    // Background
    colors[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.08f, 0.08f, 0.08f, 0.00f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    
    // Borders
    colors[ImGuiCol_Border]             = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Text
    colors[ImGuiCol_Text]               = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Headers
    colors[ImGuiCol_Header]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.46f, 0.46f, 0.46f, 1.00f);
    
    // Buttons
    colors[ImGuiCol_Button]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.46f, 0.46f, 0.46f, 1.00f);
    
    // Frame BG (inputs, sliders, etc)
    colors[ImGuiCol_FrameBg]            = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    
    // Tabs
    colors[ImGuiCol_Tab]                = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.15f, 0.15f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
    
    // Title
    colors[ImGuiCol_TitleBg]            = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.08f, 0.08f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    
    // Separator
    colors[ImGuiCol_Separator]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorActive]    = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    
    // Resize
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.26f, 0.26f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.31f, 0.31f, 0.31f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.41f, 0.41f, 0.41f, 0.95f);
    
    // Plot
    colors[ImGuiCol_PlotLines]          = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]   = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    
    // Misc
    colors[ImGuiCol_TextSelectedBg]     = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]     = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]  = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]   = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}
```

## Professional Light Theme

```cpp
void applyProfessionalLightTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Sizing (same as dark)
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    
    ImVec4* colors = style.Colors;
    
    // Light backgrounds
    colors[ImGuiCol_WindowBg]       = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_ChildBg]        = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_PopupBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    
    // Dark text
    colors[ImGuiCol_Text]           = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    
    // Subtle borders
    colors[ImGuiCol_Border]         = ImVec4(0.85f, 0.85f, 0.85f, 0.50f);
    
    // Interactive elements
    colors[ImGuiCol_Button]         = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    
    colors[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
}
```

## Accent Colors & Branding

```cpp
struct Theme {
    ImVec4 primary   = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);  // Blue
    ImVec4 secondary = ImVec4(0.78f, 0.58f, 0.96f, 1.00f);  // Purple
    ImVec4 success   = ImVec4(0.40f, 0.82f, 0.40f, 1.00f);  // Green
    ImVec4 warning   = ImVec4(1.00f, 0.78f, 0.36f, 1.00f);  // Yellow
    ImVec4 danger    = ImVec4(0.86f, 0.34f, 0.36f, 1.00f);  // Red
    
    void applyAccents() {
        ImVec4* colors = ImGui::GetStyle().Colors;
        
        // Primary accent for active elements
        colors[ImGuiCol_CheckMark]          = primary;
        colors[ImGuiCol_SliderGrab]         = primary;
        colors[ImGuiCol_SliderGrabActive]   = lighten(primary, 0.2f);
        colors[ImGuiCol_Button]             = darken(primary, 0.4f);
        colors[ImGuiCol_ButtonHovered]      = darken(primary, 0.2f);
        colors[ImGuiCol_ButtonActive]       = primary;
        colors[ImGuiCol_Header]             = withAlpha(primary, 0.31f);
        colors[ImGuiCol_HeaderHovered]      = withAlpha(primary, 0.50f);
        colors[ImGuiCol_HeaderActive]       = withAlpha(primary, 0.70f);
        colors[ImGuiCol_TabActive]          = primary;
        colors[ImGuiCol_TabUnfocusedActive] = darken(primary, 0.2f);
    }
    
    ImVec4 lighten(ImVec4 color, float amount) {
        return ImVec4(
            ImMin(color.x + amount, 1.0f),
            ImMin(color.y + amount, 1.0f),
            ImMin(color.z + amount, 1.0f),
            color.w
        );
    }
    
    ImVec4 darken(ImVec4 color, float amount) {
        return ImVec4(
            ImMax(color.x - amount, 0.0f),
            ImMax(color.y - amount, 0.0f),
            ImMax(color.z - amount, 0.0f),
            color.w
        );
    }
    
    ImVec4 withAlpha(ImVec4 color, float alpha) {
        return ImVec4(color.x, color.y, color.z, alpha);
    }
};
```

## Custom Widget Styling

```cpp
// Styled button with custom colors
bool ColoredButton(const char* label, const ImVec4& color, 
                   const ImVec2& size = ImVec2(0,0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
        ImVec4(color.x * 1.1f, color.y * 1.1f, color.z * 1.1f, color.w));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
        ImVec4(color.x * 0.9f, color.y * 0.9f, color.z * 0.9f, color.w));
    
    bool pressed = ImGui::Button(label, size);
    
    ImGui::PopStyleColor(3);
    return pressed;
}

// Gradient background
void RenderGradientBackground() {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 p_min = ImGui::GetMainViewport()->Pos;
    ImVec2 p_max = ImVec2(p_min.x + ImGui::GetIO().DisplaySize.x,
                          p_min.y + ImGui::GetIO().DisplaySize.y);
    
    ImU32 col_top = IM_COL32(18, 18, 24, 255);
    ImU32 col_bot = IM_COL32(28, 28, 38, 255);
    draw_list->AddRectFilledMultiColor(p_min, p_max, 
        col_top, col_top, col_bot, col_bot);
}

// Custom styled frame
void BeginStyledFrame(const char* label) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.16f, 0.16f, 0.60f));
    
    ImGui::BeginChildFrame(ImGui::GetID(label), ImVec2(0, 0));
}

void EndStyledFrame() {
    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
```

## Typography & Visual Hierarchy

```cpp
class Typography {
private:
    ImFont* font_regular = nullptr;
    ImFont* font_bold = nullptr;
    ImFont* font_mono = nullptr;
    ImFont* font_large = nullptr;
    ImFont* font_small = nullptr;
    
public:
    void load(float scale = 1.0f) {
        ImGuiIO& io = ImGui::GetIO();
        
        // Clear fonts
        io.Fonts->Clear();
        
        // Regular
        font_regular = io.Fonts->AddFontFromFileTTF(
            "fonts/Inter-Regular.ttf", 16.0f * scale);
            
        // Bold
        font_bold = io.Fonts->AddFontFromFileTTF(
            "fonts/Inter-Bold.ttf", 16.0f * scale);
            
        // Large (headers)
        font_large = io.Fonts->AddFontFromFileTTF(
            "fonts/Inter-Bold.ttf", 24.0f * scale);
            
        // Small (captions)
        font_small = io.Fonts->AddFontFromFileTTF(
            "fonts/Inter-Regular.ttf", 13.0f * scale);
            
        // Monospace (code)
        font_mono = io.Fonts->AddFontFromFileTTF(
            "fonts/JetBrainsMono-Regular.ttf", 14.0f * scale);
            
        io.Fonts->Build();
    }
    
    void H1(const char* text) {
        ImGui::PushFont(font_large);
        ImGui::Text("%s", text);
        ImGui::PopFont();
        ImGui::Spacing();
    }
    
    void H2(const char* text) {
        ImGui::PushFont(font_bold);
        ImGui::Text("%s", text);
        ImGui::PopFont();
    }
    
    void Body(const char* text) {
        ImGui::TextWrapped("%s", text);
    }
    
    void Caption(const char* text) {
        ImGui::PushFont(font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, 
            ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    
    void Code(const char* text) {
        ImGui::PushFont(font_mono);
        ImGui::PushStyleColor(ImGuiCol_Text, 
            ImVec4(0.9f, 0.9f, 0.4f, 1.0f));
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
};
```

## Animation & Polish

```cpp
class AnimatedUI {
private:
    std::unordered_map<ImGuiID, float> animations;
    
public:
    float animate(const char* id, float target, float speed = 8.0f) {
        ImGuiID key = ImGui::GetID(id);
        float& current = animations[key];
        
        float dt = ImGui::GetIO().DeltaTime;
        current += (target - current) * dt * speed;
        
        return current;
    }
    
    void animatedButton(const char* label) {
        ImVec2 pos = ImGui::GetCursorPos();
        bool hovered = ImGui::IsMouseHoveringRect(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + 200,
                   ImGui::GetCursorScreenPos().y + 30)
        );
        
        float hover_anim = animate(label, hovered ? 1.0f : 0.0f);
        
        // Animated background
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + 200, p_min.y + 30);
        
        ImU32 col = ImGui::GetColorU32(ImVec4(
            0.2f + hover_anim * 0.1f,
            0.2f + hover_anim * 0.1f,
            0.2f + hover_anim * 0.1f,
            1.0f
        ));
        
        draw_list->AddRectFilled(p_min, p_max, col, 4.0f);
        
        // Offset text slightly when hovered
        ImGui::SetCursorPosX(pos.x + 10 + hover_anim * 2);
        ImGui::SetCursorPosY(pos.y + 5);
        ImGui::Text("%s", label);
        
        ImGui::SetCursorPosY(pos.y + 35);
        
        // Handle click
        if (hovered && ImGui::IsMouseClicked(0)) {
            // Clicked
        }
    }
};
```

## Common UI Patterns

### Clean Settings Panel
```cpp
void renderSettings() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    
    if (ImGui::BeginChild("Settings", ImVec2(400, 0), true)) {
        Typography::H2("Audio Settings");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Category
        ImGui::TextDisabled("OUTPUT");
        ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);
        ImGui::SliderFloat("Balance", &balance, -1.0f, 1.0f);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextDisabled("INPUT");
        ImGui::Combo("Device", &selected_device, device_names, device_count);
        ImGui::SliderFloat("Gain", &gain, 0.0f, 2.0f);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Action buttons
        if (ColoredButton("Apply", Theme::success)) {
            applySettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            cancelSettings();
        }
    }
    ImGui::EndChild();
    
    ImGui::PopStyleVar();
}
```

### Modern Tabs
```cpp
void renderModernTabs() {
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("General")) {
            renderGeneralSettings();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio")) {
            renderAudioSettings();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Display")) {
            renderDisplaySettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
```

## Design Best Practices

### DO:
- ✅ Use consistent spacing (multiples of 4 or 8)
- ✅ Create visual hierarchy with font sizes/weights
- ✅ Use subtle animations for polish
- ✅ Apply consistent color scheme
- ✅ Round corners for modern look
- ✅ Add proper padding to elements
- ✅ Use shadows sparingly for depth

### DON'T:
- ❌ Use too many colors (3-5 max)
- ❌ Make borders too thick
- ❌ Use pure black/white (too harsh)
- ❌ Forget about hover states
- ❌ Make text too small (min 13px)
- ❌ Use system fonts (bundle your own)

## Quick Style Presets

```cpp
namespace Styles {
    void Minimal() {
        auto& style = ImGui::GetStyle();
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(1, 1, 1, 1);
        // ... minimal style
    }
    
    void Material() {
        // Material Design inspired
        auto& style = ImGui::GetStyle();
        style.FrameRounding = 2.0f;
        style.GrabRounding = 2.0f;
        // ... material colors
    }
    
    void Fluent() {
        // Windows 11 Fluent Design
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 11.0f;
        style.FrameRounding = 4.0f;
        // ... fluent style
    }
}
```

## Next Steps

- [Common Patterns →](patterns.md) - UI patterns and examples
- [Performance Tips →](performance.md) - Optimize your UI