# ImGui Troubleshooting Guide

## Common Issues & Solutions

### Display Issues

#### Problem: Blurry or Tiny Text
**Symptoms**: Text appears blurry, too small, or pixelated on high-DPI displays.

**Solutions**:
```cpp
// Solution 1: Load fonts at correct DPI scale
float scale = getDisplayScale();  // Get from GLFW
ImFont* font = io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f * scale);

// Solution 2: Check font atlas texture size
io.Fonts->TexDesiredWidth = 2048;  // Increase if needed

// Solution 3: Ensure proper OpenGL state
glBindTexture(GL_TEXTURE_2D, font_texture);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

#### Problem: Window Appears Off-Screen
**Symptoms**: Windows saved positions cause them to appear outside visible area.

**Solutions**:
```cpp
// Solution 1: Constrain window position
ImVec2 pos = ImGui::GetWindowPos();
ImVec2 size = ImGui::GetWindowSize();
ImGuiViewport* viewport = ImGui::GetMainViewport();

pos.x = ImMax(pos.x, viewport->Pos.x);
pos.y = ImMax(pos.y, viewport->Pos.y);
pos.x = ImMin(pos.x, viewport->Pos.x + viewport->Size.x - size.x);
pos.y = ImMin(pos.y, viewport->Pos.y + viewport->Size.y - size.y);

ImGui::SetWindowPos(pos);

// Solution 2: Reset window positions
if (ImGui::IsKeyPressed(ImGuiKey_F12)) {
    ImGui::SetWindowPos("MyWindow", ImVec2(100, 100));
}

// Solution 3: Disable saving positions
ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
```

#### Problem: Black or Corrupted Rendering
**Symptoms**: UI renders as black rectangles or corrupted graphics.

**Solutions**:
```cpp
// Solution 1: Check OpenGL state before/after
GLint last_blend_src, last_blend_dst;
GLboolean last_enable_blend;
glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
glGetIntegerv(GL_BLEND_DST, &last_blend_dst);
last_enable_blend = glIsEnabled(GL_BLEND);

// Render ImGui
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// Restore state
if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
glBlendFunc(last_blend_src, last_blend_dst);

// Solution 2: Clear errors before rendering
while (glGetError() != GL_NO_ERROR) {}  // Clear error queue

// Solution 3: Verify shader compilation
if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) {
    printf("Failed to create ImGui OpenGL objects\n");
}
```

### Input Issues

#### Problem: Mouse Input Not Working
**Symptoms**: Can't click buttons, mouse position wrong, or no hover effects.

**Solutions**:
```cpp
// Solution 1: Ensure callbacks are installed
ImGui_ImplGlfw_InitForOpenGL(window, true);  // true = install callbacks

// Solution 2: Manual callback forwarding
void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    // Forward to ImGui
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    
    // Only handle if ImGui doesn't want mouse
    if (!ImGui::GetIO().WantCaptureMouse) {
        // Your mouse handling
    }
}

// Solution 3: Check mouse position
ImGuiIO& io = ImGui::GetIO();
printf("Mouse: %.1f, %.1f\n", io.MousePos.x, io.MousePos.y);
printf("Display: %.1f x %.1f\n", io.DisplaySize.x, io.DisplaySize.y);
```

#### Problem: Keyboard Input Ignored
**Symptoms**: Can't type in text fields, shortcuts don't work.

**Solutions**:
```cpp
// Solution 1: Check input capture flags
ImGuiIO& io = ImGui::GetIO();
if (io.WantCaptureKeyboard) {
    // ImGui wants keyboard - don't process app shortcuts
}

// Solution 2: Verify key mapping
io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
// ... map all keys

// Solution 3: Debug key states
if (ImGui::Begin("Input Debug")) {
    for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++) {
        if (io.KeysDown[i]) {
            ImGui::Text("Key %d is down", i);
        }
    }
}
ImGui::End();
```

### Performance Issues

#### Problem: Low FPS with Many Windows
**Symptoms**: Frame rate drops significantly with multiple windows open.

**Solutions**:
```cpp
// Solution 1: Skip invisible windows
if (!ImGui::Begin("Window")) {
    ImGui::End();
    return;  // Skip content if collapsed
}

// Solution 2: Use child windows instead
ImGui::Begin("MainWindow");
ImGui::BeginChild("Sub1", ImVec2(200, 0), true);
// Content
ImGui::EndChild();
ImGui::End();

// Solution 3: Reduce draw calls
// Instead of many Begin/End pairs, use one window with sections
```

#### Problem: High Memory Usage
**Symptoms**: Application memory grows over time.

**Solutions**:
```cpp
// Solution 1: Check for ID stack leaks
int id_stack_size = GImGui->CurrentWindowStack.Size;
// Should be same before/after frame

// Solution 2: Clear per-frame allocations
ImGui::MemFree(ImGui::MemAlloc(0));  // Force cleanup

// Solution 3: Monitor allocations
static size_t total_alloc = 0;
ImGui::SetAllocatorFunctions(
    [](size_t sz, void*) -> void* {
        total_alloc += sz;
        return malloc(sz);
    },
    [](void* ptr, void*) {
        free(ptr);
    }
);
```

### Layout Issues

#### Problem: Widgets Don't Align Properly
**Symptoms**: Buttons, text, or other widgets appear misaligned.

**Solutions**:
```cpp
// Solution 1: Use consistent spacing
ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
// Your widgets
ImGui::PopStyleVar();

// Solution 2: Manual positioning
float pos_x = ImGui::GetCursorPosX();
ImGui::Text("Label:");
ImGui::SameLine();
ImGui::SetCursorPosX(pos_x + 100);  // Fixed position
ImGui::InputText("##input", buffer, sizeof(buffer));

// Solution 3: Use tables for alignment
if (ImGui::BeginTable("layout", 2)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Label");
    ImGui::TableNextColumn();
    ImGui::InputText("##input", buffer, sizeof(buffer));
    ImGui::EndTable();
}
```

#### Problem: Window Size Issues
**Symptoms**: Windows too small, content clipped, or unwanted scrollbars.

**Solutions**:
```cpp
// Solution 1: Set minimum size
ImGui::SetNextWindowSizeConstraints(
    ImVec2(300, 200),    // Min size
    ImVec2(FLT_MAX, FLT_MAX)  // Max size
);

// Solution 2: Auto-resize to content
ImGui::Begin("Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

// Solution 3: Calculate content size
ImVec2 content_size = ImGui::GetContentRegionAvail();
if (content_size.x < 300) {
    ImGui::SetWindowSize(ImVec2(300, -1));
}
```

### Font Issues

#### Problem: Missing Characters or Glyphs
**Symptoms**: Squares or '?' appear instead of certain characters.

**Solutions**:
```cpp
// Solution 1: Load wider character range
ImFontConfig config;
config.MergeMode = false;
io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f, &config, 
    io.Fonts->GetGlyphRangesDefault());

// Add more ranges
config.MergeMode = true;
io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f, &config,
    io.Fonts->GetGlyphRangesJapanese());

// Solution 2: Custom glyph ranges
static const ImWchar ranges[] = {
    0x0020, 0x00FF,  // Basic Latin + Latin Supplement
    0x0100, 0x017F,  // Latin Extended-A
    0x0400, 0x052F,  // Cyrillic + Supplement
    0,
};
io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f, nullptr, ranges);

// Solution 3: Fallback font
io.Fonts->AddFontDefault();  // Always have a fallback
```

### Platform-Specific Issues

#### Problem: Multi-Monitor DPI Issues (Windows)
**Solutions**:
```cpp
// Enable per-monitor DPI awareness
#ifdef _WIN32
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

// Handle DPI changes
glfwSetWindowContentScaleCallback(window, [](GLFWwindow* window, float xscale, float yscale) {
    // Reload fonts at new scale
    ImGui::GetIO().Fonts->Clear();
    LoadFontsAtScale(xscale);
    ImGui::GetIO().Fonts->Build();
});
```

#### Problem: Clipboard Not Working (Linux)
**Solutions**:
```cpp
// Solution 1: Check clipboard implementation
const char* clipboard = ImGui::GetClipboardText();
if (!clipboard) {
    printf("Clipboard backend not working\n");
}

// Solution 2: Custom clipboard handlers
io.SetClipboardTextFn = [](void*, const char* text) {
    // Your clipboard implementation
};
io.GetClipboardTextFn = [](void*) -> const char* {
    // Return clipboard content
    return clipboard_content;
};
```

## Debugging Techniques

### Visual Debugging

```cpp
// Draw widget bounds
void debugDrawBounds() {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    
    // Draw bounds of last item
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255));
}

// Show all window positions
void debugWindows() {
    ImGuiContext& g = *GImGui;
    for (ImGuiWindow* window : g.Windows) {
        ImGui::Text("%s: Pos=(%.1f,%.1f) Size=(%.1f,%.1f)",
            window->Name,
            window->Pos.x, window->Pos.y,
            window->Size.x, window->Size.y);
    }
}
```

### State Inspection

```cpp
// Check ImGui state
void inspectState() {
    ImGuiContext& g = *GImGui;
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::Text("Frame: %d", g.FrameCount);
    ImGui::Text("Windows: %d", g.Windows.Size);
    ImGui::Text("Active ID: 0x%08X", g.ActiveId);
    ImGui::Text("Hovered ID: 0x%08X", g.HoveredId);
    ImGui::Text("Mouse: %.1f, %.1f", io.MousePos.x, io.MousePos.y);
    ImGui::Text("Want Capture Mouse: %s", io.WantCaptureMouse ? "Yes" : "No");
    ImGui::Text("Want Capture Keyboard: %s", io.WantCaptureKeyboard ? "Yes" : "No");
}
```

## Error Recovery

### Safe Initialization

```cpp
bool initImGuiSafely() {
    try {
        IMGUI_CHECKVERSION();
        ImGuiContext* ctx = ImGui::CreateContext();
        if (!ctx) {
            fprintf(stderr, "Failed to create ImGui context\n");
            return false;
        }
        
        ImGuiIO& io = ImGui::GetIO();
        
        // Validate display size
        if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0) {
            fprintf(stderr, "Invalid display size\n");
            return false;
        }
        
        // Initialize backends with error checking
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            fprintf(stderr, "Failed to init GLFW backend\n");
            return false;
        }
        
        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            fprintf(stderr, "Failed to init OpenGL backend\n");
            ImGui_ImplGlfw_Shutdown();
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ImGui init exception: %s\n", e.what());
        return false;
    }
}
```

### Crash Prevention

```cpp
// Wrap ImGui calls for safety
class SafeImGui {
public:
    static bool Begin(const char* name, bool* open = nullptr, 
                     ImGuiWindowFlags flags = 0) {
        try {
            return ImGui::Begin(name, open, flags);
        }
        catch (...) {
            fprintf(stderr, "ImGui::Begin crashed for window '%s'\n", name);
            return false;
        }
    }
    
    static void End() {
        try {
            ImGui::End();
        }
        catch (...) {
            fprintf(stderr, "ImGui::End crashed\n");
        }
    }
};
```

## Quick Reference

### Debug Checklist
- [ ] Check `io.DisplaySize` is valid
- [ ] Verify font atlas was built
- [ ] Ensure callbacks are forwarded
- [ ] Check OpenGL errors
- [ ] Verify ImGui context exists
- [ ] Test with default style
- [ ] Try without custom rendering
- [ ] Check platform-specific settings

### Common Mistakes
- 🚫 Forgetting `ImGui::End()` for each `Begin()`
- 🚫 Not checking if window is visible before rendering
- 🚫 Modifying style/colors without pushing/popping
- 🚫 Using same ID for multiple widgets
- 🚫 Processing input when ImGui wants it
- 🚫 Not handling DPI scaling
- 🚫 Assuming coordinate systems match