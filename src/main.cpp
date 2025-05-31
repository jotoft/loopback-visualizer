#include <iostream>
#include <audio_loopback/loopback_recorder.h>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/audio_capture.h>
#include <chrono>
#include <thread>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <vector>
#include "core/result.h"
#include "core/option.h"
#include "core/unit.h"
#include "visualization/phase_lock_analyzer.h"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

const uint32_t width = 2400;

class Initializer {
public:
    Initializer() { std::ios_base::sync_with_stdio(false); }
};

struct vec4 {
    float x, y, z, w;
};

// Display buffer
const uint32_t DISPLAY_SAMPLES = width;
static vec4 display_buffer[DISPLAY_SAMPLES];
static float audio_read_buffer[1024];

// GUI state
struct GuiState {
    bool phase_lock_enabled = true;  // Enable by default
    bool show_demo_window = false;
    bool show_correlation_graph = true;
    
    // Visual settings
    ImVec4 waveform_color = ImVec4(0.0f, 1.0f, 0.9f, 1.0f);
    ImVec4 good_lock_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    ImVec4 moderate_lock_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    ImVec4 poor_lock_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
};

static GuiState gui_state;

// App state for callbacks
struct AppState {
    bool capture_input = false;
    std::unique_ptr<audio::AudioCapture>* audio_capture_ptr = nullptr;
    std::vector<audio::AudioSinkInfo> available_devices;
    audio::AudioSinkInfo current_device;
};

auto load_file(const std::string &filename) -> core::Result<std::string, std::string> {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<std::string, std::string>::Err("Failed to open file: " + filename);
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    
    if (file.bad()) {
        return core::Result<std::string, std::string>::Err("Error reading file: " + filename);
    }
    
    return core::Result<std::string, std::string>::Ok(ss.str());
}

auto check_shader_compilation(uint32_t shader) -> core::Result<core::Unit, std::string> {
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        return core::Result<core::Unit, std::string>::Err(
            "Shader compilation failed: " + std::string(infoLog)
        );
    }
    
    return core::Result<core::Unit, std::string>::Ok(core::unit);
}

auto check_shader_link(uint32_t shader) -> core::Result<core::Unit, std::string> {
    int success;
    char infoLog[512];
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    
    if (!success) {
        glGetProgramInfoLog(shader, 512, NULL, infoLog);
        return core::Result<core::Unit, std::string>::Err(
            "Shader linking failed: " + std::string(infoLog)
        );
    }
    
    return core::Result<core::Unit, std::string>::Ok(core::unit);
}

int main() {
    Initializer _init;
    AppState app_state;
    
    // Audio capture pointer
    std::unique_ptr<audio::AudioCapture> audio_capture;
    app_state.audio_capture_ptr = &audio_capture;
    
    // Get available devices
    auto devices_result = audio::list_sinks();
    if (devices_result.is_ok()) {
        app_state.available_devices = devices_result.unwrap();
    }
    
    // Add default input device
    auto input_device = audio::get_default_sink(true);
    if (input_device.is_some()) {
        app_state.available_devices.push_back(input_device.unwrap());
    }
    
    // Function to switch audio source
    auto switch_audio_source = [&app_state, &audio_capture](const audio::AudioSinkInfo* device = nullptr) {
        if (audio_capture) {
            audio_capture->stop();
            audio_capture.reset();
        }
        
        audio::AudioSinkInfo selected_device;
        
        if (device) {
            selected_device = *device;
            app_state.current_device = selected_device;
        } else {
            app_state.capture_input = !app_state.capture_input;
            auto device_opt = audio::get_default_sink(app_state.capture_input);
            if (device_opt.is_none()) {
                std::cerr << "No default " << (app_state.capture_input ? "input" : "sink") << " found" << std::endl;
                return false;
            }
            selected_device = device_opt.unwrap();
            app_state.current_device = selected_device;
        }
        
        std::cout << "\nSwitching to: " << selected_device.name << std::endl;
        
        audio_capture = audio::create_audio_capture(selected_device);
        auto result = audio_capture->start();
        if (result.is_err()) {
            std::cerr << "Failed to start audio capture" << std::endl;
            return false;
        }
        
        return true;
    };
    
    // Initialize with loopback (system audio)
    if (!switch_audio_source()) {
        return -1;
    }
    app_state.capture_input = false;

    // Load shaders
    auto soundwave_result = load_file("shaders/soundwave_optimized.glsl");
    auto vertex_result = load_file("shaders/basic_vertex.glsl");

    if (soundwave_result.is_err()) {
        std::cerr << "Failed to load soundwave shader: " << soundwave_result.error() << std::endl;
        return -1;
    }

    if (vertex_result.is_err()) {
        std::cerr << "Failed to load vertex shader: " << vertex_result.error() << std::endl;
        return -1;
    }

    auto soundwave_shader_text = std::move(soundwave_result).unwrap();
    auto basic_vertex_text = std::move(vertex_result).unwrap();

    // Initialize GLFW
    if (!glfwInit()) return -1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(width, 900, "Audio Visualizer", NULL, NULL);
    
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    
    // Calculate DPI scale
    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    float dpi_scale = (float)fb_width / (float)window_width;
    
    std::cout << "Window size: " << width << "x900, Framebuffer: " << fb_width << "x" << fb_height << std::endl;
    std::cout << "DPI scale: " << dpi_scale << std::endl;
    
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Load font at higher resolution for HiDPI
    io.Fonts->Clear();
    ImFontConfig font_config;
    font_config.OversampleH = 3;
    font_config.OversampleV = 3;
    font_config.PixelSnapH = true;
    
    float font_size = 13.0f * dpi_scale;
    io.Fonts->AddFontFromFileTTF("/usr/share/fonts/liberation/LiberationSans-Regular.ttf", font_size, &font_config);
    
    if (io.Fonts->Fonts.Size == 0) {
        font_config.SizePixels = font_size;
        io.Fonts->AddFontDefault(&font_config);
    }
    
    io.Fonts->Build();
    io.FontGlobalScale = 1.0f;
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    // Create OpenGL resources
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Compile shaders
    uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
    auto vertex_src = basic_vertex_text.c_str();
    glShaderSource(vertexShader, 1, &vertex_src, NULL);
    glCompileShader(vertexShader);

    if (auto result = check_shader_compilation(vertexShader); result.is_err()) {
        std::cerr << "Vertex shader compilation failed: " << result.error() << std::endl;
        return -1;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    auto fragment_src = soundwave_shader_text.c_str();
    glShaderSource(fragmentShader, 1, &fragment_src, NULL);
    glCompileShader(fragmentShader);

    if (auto result = check_shader_compilation(fragmentShader); result.is_err()) {
        std::cerr << "Fragment shader compilation failed: " << result.error() << std::endl;
        return -1;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    if (auto result = check_shader_link(shaderProgram); result.is_err()) {
        std::cerr << "Shader linking failed: " << result.error() << std::endl;
        return -1;
    }

    glUseProgram(shaderProgram);
    
    // Get uniform locations
    GLint resolution_loc = glGetUniformLocation(shaderProgram, "resolution");
    GLint sample_loc = glGetUniformLocation(shaderProgram, "current_sample");
    GLint trigger_level_loc = glGetUniformLocation(shaderProgram, "trigger_level");
    GLint phase_lock_enabled_loc = glGetUniformLocation(shaderProgram, "phase_lock_enabled");

    // Create UBO for samples
    unsigned int ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(display_buffer), nullptr, GL_DYNAMIC_DRAW);
    
    unsigned int block_index = glGetUniformBlockIndex(shaderProgram, "SamplesBlock");
    GLuint binding_point_index = 2;
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point_index, ubo);
    glUniformBlockBinding(shaderProgram, block_index, binding_point_index);

    glfwSwapInterval(0); // Disable VSync for minimum latency
    
    // Create phase lock analyzer
    visualization::PhaseLockAnalyzer::Config analyzer_config;
    analyzer_config.phase_smoothing = 0.0f;
    analyzer_config.correlation_threshold = 0.45f;
    analyzer_config.correlation_window_size = 300;
    analyzer_config.display_samples = DISPLAY_SAMPLES;
    
    visualization::PhaseLockAnalyzer phase_analyzer(analyzer_config);
    
    // FPS calculation
    double last_fps_time = glfwGetTime();
    int fps_frame_count = 0;
    float current_fps = 0.0f;
    
    // Audio stats
    size_t audio_buffer_size = 0;
    size_t overruns = 0;
    size_t underruns = 0;
    
    // Initialize display buffer
    std::memset(display_buffer, 0, sizeof(display_buffer));
    glViewport(0, 0, fb_width, fb_height);
    
    // Main loop
    using clock = std::chrono::high_resolution_clock;
    using microseconds = std::chrono::microseconds;
    constexpr auto frame_time = microseconds(4167); // 240 FPS
    auto next_frame = clock::now();
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Update viewport
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glUniform2f(resolution_loc, (float)fb_width, (float)fb_height);
        
        // Read audio samples
        size_t available = audio_capture->available_samples();
        audio_buffer_size = available;
        if (available > 512) available = 512;
        size_t samples_read = audio_capture->read_samples(audio_read_buffer, available);
        
        if (samples_read > 0) {
            phase_analyzer.add_samples(audio_read_buffer, samples_read);
        }
        
        // Update analyzer config if changed
        auto& config = phase_analyzer.get_config();
        bool config_changed = false;
        
        // Analyze phase
        auto state = phase_analyzer.analyze(gui_state.phase_lock_enabled);
        
        // Fill display buffer from phase buffer
        const float* phase_buffer = phase_analyzer.get_phase_buffer();
        size_t read_pos = state.read_position;
        
        for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
            display_buffer[i].x = phase_buffer[read_pos];
            display_buffer[i].y = 0.0f;
            display_buffer[i].z = 0.0f;
            display_buffer[i].w = 0.0f;
            read_pos = (read_pos + 1) % phase_analyzer.get_phase_buffer_size();
        }
        
        // Upload to GPU
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        void* ptr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            memcpy(ptr, display_buffer, sizeof(display_buffer));
            glUnmapBuffer(GL_UNIFORM_BUFFER);
        }
        
        // Set uniforms
        glUniform1i(sample_loc, 0);
        glUniform1f(trigger_level_loc, state.best_correlation);
        glUniform1i(phase_lock_enabled_loc, gui_state.phase_lock_enabled ? 1 : 0);
        
        // Clear and draw
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // ImGui rendering
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
            
            ImGui::Begin("Audio Visualizer Controls");
            
            ImGui::Text("FPS: %.1f", current_fps);
            ImGui::Text("Audio Buffer: %zu samples", audio_buffer_size);
            ImGui::Text("Latency: ~%.1f ms", audio_buffer_size / 44.1);
            
            ImGui::Separator();
            
            // Phase Lock Settings
            if (ImGui::CollapsingHeader("Phase Lock Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Phase Lock", &gui_state.phase_lock_enabled);
                
                if (gui_state.phase_lock_enabled) {
                    ImGui::Text("Correlation: %.2f", state.best_correlation);
                    ImGui::Text("Lock Status: %s", state.has_lock ? "LOCKED" : "SEARCHING");
                    
                    auto config = phase_analyzer.get_config();
                    
                    if (ImGui::SliderFloat("Phase Smoothing", &config.phase_smoothing, 0.0f, 0.99f)) {
                        phase_analyzer.set_config(config);
                    }
                    if (ImGui::SliderFloat("Correlation Threshold", &config.correlation_threshold, 0.3f, 0.95f)) {
                        phase_analyzer.set_config(config);
                    }
                    if (ImGui::SliderInt("Correlation Window", &config.correlation_window_size, 128, 1024)) {
                        phase_analyzer.set_config(config);
                    }
                    
                    if (ImGui::Button("Reset Reference")) {
                        phase_analyzer.reset();
                    }
                }
            }
            
            // Audio Device Selection
            if (ImGui::CollapsingHeader("Audio Devices", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Current: %s", app_state.current_device.name.c_str());
                ImGui::Separator();
                
                for (size_t i = 0; i < app_state.available_devices.size(); ++i) {
                    const auto& device = app_state.available_devices[i];
                    bool is_selected = (device.device_id == app_state.current_device.device_id);
                    
                    std::string label = device.name;
                    label += device.capture_device ? " [INPUT]" : " [OUTPUT]";
                    
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        switch_audio_source(&device);
                    }
                }
                
                ImGui::Separator();
                if (ImGui::Button("Refresh Device List")) {
                    auto devices_result = audio::list_sinks();
                    if (devices_result.is_ok()) {
                        app_state.available_devices = devices_result.unwrap();
                        auto input_device = audio::get_default_sink(true);
                        if (input_device.is_some()) {
                            app_state.available_devices.push_back(input_device.unwrap());
                        }
                    }
                }
            }
            
            // Visual Settings
            if (ImGui::CollapsingHeader("Visual Settings")) {
                ImGui::ColorEdit3("Waveform Color", (float*)&gui_state.waveform_color);
                ImGui::ColorEdit3("Good Lock Color", (float*)&gui_state.good_lock_color);
                ImGui::ColorEdit3("Moderate Lock Color", (float*)&gui_state.moderate_lock_color);
                ImGui::ColorEdit3("Poor Lock Color", (float*)&gui_state.poor_lock_color);
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Show Demo Window", &gui_state.show_demo_window);
            ImGui::Checkbox("Show Correlation Graph", &gui_state.show_correlation_graph);
            
            ImGui::End();
            
            // Correlation graph
            if (gui_state.show_correlation_graph && gui_state.phase_lock_enabled) {
                ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Correlation History");
                
                const auto& history = phase_analyzer.get_correlation_history();
                if (!history.empty()) {
                    std::vector<float> values(history.begin(), history.end());
                    ImGui::PlotLines("Correlation", values.data(), values.size(), 
                                     0, nullptr, 0.0f, 1.0f, ImVec2(0, 150));
                }
                
                ImGui::End();
            }
            
            if (gui_state.show_demo_window) {
                ImGui::ShowDemoWindow(&gui_state.show_demo_window);
            }
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        
        // Frame timing
        next_frame += frame_time;
        auto now = clock::now();
        if (next_frame > now) {
            std::this_thread::sleep_until(next_frame);
        } else {
            next_frame = now;
        }
        
        // FPS calculation
        fps_frame_count++;
        double current_time = glfwGetTime();
        double fps_delta = current_time - last_fps_time;
        
        if (fps_delta >= 0.5) {
            current_fps = fps_frame_count / fps_delta;
            auto stats = audio_capture->get_stats();
            overruns = stats.overruns;
            underruns = stats.underruns;
            
            fps_frame_count = 0;
            last_fps_time = current_time;
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    audio_capture->stop();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}