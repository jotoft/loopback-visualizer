# ImGui Setup & Integration

## Basic Setup with GLFW + OpenGL3

### 1. Include Dependencies

```cpp
// OpenGL loader (before GLFW)
#include <glad/gl.h>

// Platform
#include <GLFW/glfw3.h>

// ImGui core
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
```

### 2. Initialization

```cpp
// After creating GLFW window and OpenGL context
GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui App", NULL, NULL);
glfwMakeContextCurrent(window);
gladLoadGL(glfwGetProcAddress);

// Setup Dear ImGui context
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO(); (void)io;

// Configuration flags
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewports

// Setup Platform/Renderer backends
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 330");

// Setup style (before loading fonts!)
ImGui::StyleColorsDark();
```

### 3. Font Loading

```cpp
// IMPORTANT: Load fonts BEFORE first frame!
ImGuiIO& io = ImGui::GetIO();

// Default font (required)
io.Fonts->AddFontDefault();

// Custom fonts with size
ImFont* font_regular = io.Fonts->AddFontFromFileTTF(
    "assets/fonts/Roboto-Regular.ttf", 16.0f);
    
ImFont* font_large = io.Fonts->AddFontFromFileTTF(
    "assets/fonts/Roboto-Bold.ttf", 24.0f);

// For HiDPI support (see hidpi-scaling.md)
float dpi_scale = getDisplayScale();
ImFont* font_hidpi = io.Fonts->AddFontFromFileTTF(
    "assets/fonts/Roboto-Regular.ttf", 16.0f * dpi_scale);
```

### 4. Main Loop

```cpp
while (!glfwWindowShouldClose(window)) {
    // Poll events
    glfwPollEvents();
    
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Your ImGui code here
    RenderUI();
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render ImGui
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Update and render additional viewports
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
    
    glfwSwapBuffers(window);
}
```

### 5. Cleanup

```cpp
// Cleanup
ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();

glfwDestroyWindow(window);
glfwTerminate();
```

## CMake Integration

```cmake
# Find packages
find_package(OpenGL REQUIRED)

# ImGui library
add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_widgets.cpp
    # Backends
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

# Your application
target_link_libraries(your_app PRIVATE imgui)
```

## Essential Configuration

### 1. Window Flags
```cpp
ImGuiWindowFlags window_flags = 0;
window_flags |= ImGuiWindowFlags_NoTitleBar;
window_flags |= ImGuiWindowFlags_NoResize;
window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
window_flags |= ImGuiWindowFlags_NoBackground;
window_flags |= ImGuiWindowFlags_NoSavedSettings;

ImGui::Begin("Window", nullptr, window_flags);
```

### 2. Input Configuration
```cpp
ImGuiIO& io = ImGui::GetIO();

// Keyboard navigation
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

// Mouse
io.MouseDrawCursor = true;  // ImGui renders cursor
io.ConfigWindowsMoveFromTitleBarOnly = true;

// Keyboard shortcuts
io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
// ... map other keys
```

### 3. Timing and Frame Rate
```cpp
// For high refresh displays
glfwSwapInterval(0);  // Disable vsync
// or
glfwSwapInterval(1);  // Enable vsync

// Delta time for animations
io.DeltaTime = 1.0f / 60.0f;  // Or calculate actual
```

## Common Integration Issues

### Issue: Blurry Text
```cpp
// Solution: Proper DPI handling
float xscale, yscale;
glfwGetWindowContentScale(window, &xscale, &yscale);
io.FontGlobalScale = xscale;  // Quick fix

// Better: Load fonts at correct size (see hidpi-scaling.md)
```

### Issue: Input Not Working
```cpp
// Ensure callbacks are installed
ImGui_ImplGlfw_InitForOpenGL(window, true);  // true = install callbacks

// Or manual callbacks
glfwSetKeyCallback(window, ImGui_ImplGlfw_KeyCallback);
glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
```

### Issue: OpenGL State Conflicts
```cpp
// Save/restore OpenGL state
GLint last_viewport[4];
glGetIntegerv(GL_VIEWPORT, last_viewport);
GLint last_blend_src, last_blend_dst;
glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
glGetIntegerv(GL_BLEND_DST, &last_blend_dst);

// Render ImGui
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// Restore state
glViewport(last_viewport[0], last_viewport[1], 
           last_viewport[2], last_viewport[3]);
glBlendFunc(last_blend_src, last_blend_dst);
```

## Minimal Complete Example

```cpp
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Example", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // GUI
        ImGui::Begin("Hello");
        ImGui::Text("Hello, world!");
        if (ImGui::Button("Click me")) {
            printf("Clicked!\n");
        }
        ImGui::End();
        
        // Render
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
```

## Next Steps

- [HiDPI & Scaling →](hidpi-scaling.md) - Essential for modern displays
- [Styling Guide →](styling-guide.md) - Make it look professional