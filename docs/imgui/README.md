# Dear ImGui v1.91.6 - Comprehensive Guide

This guide covers Dear ImGui (v1.91.6) usage in our audio visualizer project, with emphasis on proper design, HiDPI support, and practical patterns.

## Table of Contents

1. [Overview & Architecture](overview.md)
2. [Getting Started & Setup](setup.md)
3. [HiDPI & Scaling](hidpi-scaling.md)
4. [Input Handling](input-handling.md)
5. [Styling & Theming](styling-guide.md)
6. [Common Patterns](patterns.md)
7. [Performance Tips](performance.md)
8. [Troubleshooting](troubleshooting.md)

## Quick Start

```cpp
// Basic ImGui window
if (ImGui::Begin("My Window")) {
    ImGui::Text("Hello, world!");
    if (ImGui::Button("Click me")) {
        // Handle click
    }
}
ImGui::End();
```

## Why This Guide?

Dear ImGui is powerful but has quirks:
- Documentation is scattered across demos, headers, and wikis
- Default styling looks dated and needs significant work
- HiDPI support requires manual configuration
- Many patterns aren't obvious from the API

This guide consolidates best practices specific to our use case.

## Backend Configuration

We use:
- **GLFW** for windowing
- **OpenGL 3.3+** for rendering
- **imgui_impl_glfw** + **imgui_impl_opengl3** backends

## Key Concepts

### Immediate Mode
ImGui is immediate mode - UI code runs every frame:
```cpp
// This runs 60+ times per second
if (ImGui::Button("Click")) {
    // Clicked THIS frame
}
```

### ID Stack
ImGui uses string hashes for widget IDs:
```cpp
// These buttons conflict (same ID)
ImGui::Button("Settings");
ImGui::Button("Settings");  // Won't work!

// Solutions:
ImGui::Button("Settings##1");
ImGui::Button("Settings##2");
// or
ImGui::PushID(0);
ImGui::Button("Settings");
ImGui::PopID();
```

### State Management
Your app manages state, ImGui just displays:
```cpp
static float volume = 0.5f;
ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);
// 'volume' persists between frames
```

## Navigation

- [Overview & Architecture →](overview.md)