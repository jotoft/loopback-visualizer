#include <iostream>
#include <audio_loopback/loopback_recorder.h>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/audio_capture.h>
#include <chrono>
#include <thread>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <vector>
#include "core/result.h"
#include "core/option.h"
#include "core/unit.h"
#include "visualization/phase_lock_analyzer.h"
#include "visualization/frequency_analyzer.h"
#include "visualization/simple_filters.h"
#include "shaders_embedded.h"

// ImGui includes
#include <fstream>

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

// Ghost trail buffers
const int MAX_GHOST_TRAILS = 64;
static vec4 ghost_trail_buffers[MAX_GHOST_TRAILS][DISPLAY_SAMPLES];
static int ghost_trail_write_index = 0;
static int frames_since_trail_update = 0;

// GUI state
struct GuiState {
    bool phase_lock_enabled = true;  // Enable by default
    bool show_demo_window = false;
    bool show_correlation_graph = true;
    bool show_reference_waveform = true;
    bool show_spectrum_analyzer = true;
    bool show_frequency_peaks = true;
    bool show_filtered_waveform = true;
    bool show_simple_filters = true;
    bool show_reference_as_main = true;  // Show reference waveform as main display
    bool ghost_trails_enabled = false;
    float ghost_fade_speed = 0.02f;
    int ghost_trail_count = 64;
    
    // Template bank settings
    enum class TemplateBankMode {
        BANK1_ONLY,
        BANK2_ONLY,
        BANK3_ONLY,
        ALL_LAYERED,
        ALL_SEPARATE,  // Draw all 3 banks separately
        BLEND_ADD,
        BLEND_MAX
    };
    TemplateBankMode template_mode = TemplateBankMode::ALL_LAYERED;
    bool show_bank2_controls = false;
    bool show_bank3_controls = false;
    
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
    
    // Initialize with loopback (system audio output)
    app_state.capture_input = true;  // Set to true so toggle makes it false (output)
    if (!switch_audio_source()) {
        return -1;
    }
    // Now capture_input is false, meaning we're capturing system output

    // Use embedded shaders
    std::string soundwave_shader_text(shaders::soundwave_optimized_shader);
    std::string basic_vertex_text(shaders::basic_vertex_shader);

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
    
    // Store window state for fullscreen toggle
    struct WindowState {
        int x, y, width, height;
        bool is_fullscreen = false;
    } window_state;
    
    // Get initial window position and size
    glfwGetWindowPos(window, &window_state.x, &window_state.y);
    glfwGetWindowSize(window, &window_state.width, &window_state.height);
    
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
    GLint waveform_alpha_loc = glGetUniformLocation(shaderProgram, "waveform_alpha");
    GLint waveform_color_loc = glGetUniformLocation(shaderProgram, "waveform_color");
    GLint reference_mode_loc = glGetUniformLocation(shaderProgram, "reference_mode");

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
    
    // Create phase lock analyzer for bank 1 (sub-bass/bass)
    visualization::PhaseLockAnalyzer::Config analyzer_config;
    analyzer_config.phase_smoothing = 0.0f;
    analyzer_config.correlation_threshold = 0.15f;  // Lower threshold
    analyzer_config.correlation_window_size = 2048;  // Large window for low frequencies
    analyzer_config.display_samples = DISPLAY_SAMPLES;
    analyzer_config.phase_buffer_size = 4096;  // 2x correlation window
    analyzer_config.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;  // Default to EMA
    
    visualization::PhaseLockAnalyzer phase_analyzer(analyzer_config);
    
    // Create second phase lock analyzer for template bank 2
    visualization::PhaseLockAnalyzer::Config analyzer_config2;
    analyzer_config2.phase_smoothing = 0.0f;
    analyzer_config2.correlation_threshold = 0.15f;
    analyzer_config2.correlation_window_size = 1024;  // Medium window for mid-bass
    analyzer_config2.display_samples = DISPLAY_SAMPLES;
    analyzer_config2.phase_buffer_size = 4096;  // Minimum 4096 for display + search
    analyzer_config2.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;
    
    visualization::PhaseLockAnalyzer phase_analyzer2(analyzer_config2);
    phase_analyzer2.set_ema_alpha(0.12f); // Medium adaptation speed
    
    // Create third phase lock analyzer for template bank 3
    visualization::PhaseLockAnalyzer::Config analyzer_config3;
    analyzer_config3.phase_smoothing = 0.0f;
    analyzer_config3.correlation_threshold = 0.15f;
    analyzer_config3.correlation_window_size = 512;  // Small window for mids
    analyzer_config3.display_samples = DISPLAY_SAMPLES;
    analyzer_config3.phase_buffer_size = 4096;  // Minimum 4096 for display + search
    analyzer_config3.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;
    
    visualization::PhaseLockAnalyzer phase_analyzer3(analyzer_config3);
    phase_analyzer3.set_ema_alpha(0.15f); // Faster adaptation for higher frequencies
    
    // Create frequency analyzer
    visualization::FrequencyAnalyzer::Config freq_config;
    freq_config.fft_size = 2048;
    freq_config.sample_rate = 44100.0f;  // Assuming 44.1kHz
    freq_config.history_size = 100;
    freq_config.peak_threshold = 0.05f;
    freq_config.max_peaks = 5;
    
    visualization::FrequencyAnalyzer freq_analyzer(freq_config);
    
    // Create simple filters for bank 1 (40Hz+ - sub-bass/bass)
    visualization::SimpleFilters::Config filter_config;
    filter_config.sample_rate = 44100.0f;
    filter_config.highpass_enabled = true;
    filter_config.highpass_cutoff = 40.0f;   // Filter out sub-40Hz
    filter_config.highpass_resonance = 0.5f;
    filter_config.lowpass_enabled = false;   // No low-pass, catch everything above 40Hz
    visualization::SimpleFilters simple_filters(filter_config);
    
    // Create simple filters for bank 2 (160Hz+ - mids)
    visualization::SimpleFilters::Config filter_config2;
    filter_config2.sample_rate = 44100.0f;
    filter_config2.highpass_enabled = true;
    filter_config2.highpass_cutoff = 160.0f;  // Filter out sub-160Hz
    filter_config2.highpass_resonance = 0.7f;
    filter_config2.lowpass_enabled = false;  // No low-pass, catch everything above 160Hz
    visualization::SimpleFilters simple_filters2(filter_config2);
    
    // Create simple filters for bank 3 (640Hz+ - upper mids and highs)
    visualization::SimpleFilters::Config filter_config3;
    filter_config3.sample_rate = 44100.0f;
    filter_config3.highpass_enabled = true;
    filter_config3.highpass_cutoff = 640.0f; // Filter out sub-640Hz
    filter_config3.highpass_resonance = 0.7f;
    filter_config3.lowpass_enabled = false;  // No low-pass, catch everything above 640Hz
    visualization::SimpleFilters simple_filters3(filter_config3);
    
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
        
        // Handle fullscreen toggle (F11 or Alt+Enter)
        if ((glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) ||
            (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && 
             (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || 
              glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS))) {
            static bool key_was_pressed = false;
            if (!key_was_pressed) {
                key_was_pressed = true;
                
                if (!window_state.is_fullscreen) {
                    // Save current window position and size
                    glfwGetWindowPos(window, &window_state.x, &window_state.y);
                    glfwGetWindowSize(window, &window_state.width, &window_state.height);
                    
                    // Get primary monitor and its video mode
                    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                    
                    // Set windowed fullscreen
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    window_state.is_fullscreen = true;
                } else {
                    // Restore windowed mode
                    glfwSetWindowMonitor(window, nullptr, window_state.x, window_state.y, 
                                       window_state.width, window_state.height, 0);
                    window_state.is_fullscreen = false;
                }
            }
        } else if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_RELEASE &&
                   glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE) {
            static bool key_was_pressed = true;
            key_was_pressed = false;
        }
        
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
            // Process bank 1
            auto& filter_cfg = simple_filters.get_config();
            if (filter_cfg.highpass_enabled || filter_cfg.lowpass_enabled || filter_cfg.deesser_enabled) {
                // Create a copy for filtering
                std::vector<float> filtered_samples(audio_read_buffer, audio_read_buffer + samples_read);
                simple_filters.process(filtered_samples.data(), filtered_samples.size());
                
                // Use filtered samples for phase analysis
                phase_analyzer.add_samples(filtered_samples.data(), samples_read);
            } else {
                // No filters - use raw samples
                phase_analyzer.add_samples(audio_read_buffer, samples_read);
            }
            
            // Process bank 2
            auto& filter_cfg2 = simple_filters2.get_config();
            if (filter_cfg2.highpass_enabled || filter_cfg2.lowpass_enabled || filter_cfg2.deesser_enabled) {
                // Create a copy for filtering
                std::vector<float> filtered_samples2(audio_read_buffer, audio_read_buffer + samples_read);
                simple_filters2.process(filtered_samples2.data(), filtered_samples2.size());
                
                // Apply gain to compensate for high-pass filtering
                float gain2 = 2.0f;  // Boost signal after filtering
                for (size_t i = 0; i < samples_read; ++i) {
                    filtered_samples2[i] *= gain2;
                }
                
                // Use filtered samples for phase analysis
                phase_analyzer2.add_samples(filtered_samples2.data(), samples_read);
            } else {
                // No filters - use raw samples
                phase_analyzer2.add_samples(audio_read_buffer, samples_read);
            }
            
            // Process bank 3
            auto& filter_cfg3 = simple_filters3.get_config();
            if (filter_cfg3.highpass_enabled || filter_cfg3.lowpass_enabled || filter_cfg3.deesser_enabled) {
                // Create a copy for filtering
                std::vector<float> filtered_samples3(audio_read_buffer, audio_read_buffer + samples_read);
                simple_filters3.process(filtered_samples3.data(), filtered_samples3.size());
                
                // Apply gain to compensate for high-pass filtering
                float gain3 = 3.0f;  // More boost for higher frequencies
                for (size_t i = 0; i < samples_read; ++i) {
                    filtered_samples3[i] *= gain3;
                }
                
                // Use filtered samples for phase analysis
                phase_analyzer3.add_samples(filtered_samples3.data(), samples_read);
            } else {
                // No filters - use raw samples
                phase_analyzer3.add_samples(audio_read_buffer, samples_read);
            }
            
            // Use unfiltered for frequency analysis (to see full spectrum)
            freq_analyzer.process_samples(audio_read_buffer, samples_read);
        }
        
        // Update analyzer config if changed
        auto& config = phase_analyzer.get_config();
        bool config_changed = false;
        
        // Analyze all banks
        auto state = phase_analyzer.analyze(gui_state.phase_lock_enabled);
        auto state2 = phase_analyzer2.analyze(gui_state.phase_lock_enabled);
        auto state3 = phase_analyzer3.analyze(gui_state.phase_lock_enabled);
        
        // Determine which analyzer to use based on template mode
        visualization::PhaseLockAnalyzer* active_analyzer = nullptr;
        visualization::PhaseLockAnalyzer::State* active_state = nullptr;
        
        if (gui_state.template_mode == GuiState::TemplateBankMode::BANK1_ONLY) {
            active_analyzer = &phase_analyzer;
            active_state = &state;
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY) {
            active_analyzer = &phase_analyzer2;
            active_state = &state2;
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY) {
            active_analyzer = &phase_analyzer3;
            active_state = &state3;
        }
        
        // Fill display buffer
        std::vector<float> temp_samples(DISPLAY_SAMPLES, 0.0f);
        std::vector<float> temp_samples2(DISPLAY_SAMPLES, 0.0f);
        std::vector<float> temp_samples3(DISPLAY_SAMPLES, 0.0f);
        
        if (gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD || 
            gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX ||
            gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ||
            gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
            // We'll need all banks for blending/layering
            active_analyzer = &phase_analyzer;  // Use bank 1 as primary for now
            active_state = &state;
        }
        
        if (gui_state.show_reference_as_main && gui_state.phase_lock_enabled) {
            // For ALL_SEPARATE mode, we need to fill all three buffers
            if (gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
                // Fill all three reference buffers
                struct {
                    visualization::PhaseLockAnalyzer* analyzer;
                    float* buffer;
                } banks[] = {
                    {&phase_analyzer, temp_samples.data()},
                    {&phase_analyzer2, temp_samples2.data()},
                    {&phase_analyzer3, temp_samples3.data()}
                };
                
                for (auto& bank : banks) {
                    if (bank.analyzer->has_reference()) {
                        const float* ref_window = bank.analyzer->get_reference_window();
                        const auto& config = bank.analyzer->get_config();
                        int ref_size = config.correlation_window_size;
                        
                        // Stretch the pattern with cubic interpolation to fill display
                        float scale = (float)(ref_size - 1) / (float)(DISPLAY_SAMPLES - 1);
                        
                        for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                            float pos = i * scale;
                            int idx = (int)pos;
                            float frac = pos - idx;
                            
                            if (idx < ref_size - 3) {
                                // Cubic interpolation for smoother waveform
                                float y0 = (idx > 0) ? ref_window[idx - 1] : ref_window[0];
                                float y1 = ref_window[idx];
                                float y2 = ref_window[idx + 1];
                                float y3 = (idx < ref_size - 2) ? ref_window[idx + 2] : ref_window[ref_size - 1];
                                
                                float a0 = y3 - y2 - y0 + y1;
                                float a1 = y0 - y1 - a0;
                                float a2 = y2 - y0;
                                float a3 = y1;
                                
                                bank.buffer[i] = a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
                            } else {
                                // Linear interpolation at edges
                                int idx_safe = std::min(idx, ref_size - 2);
                                bank.buffer[i] = ref_window[idx_safe] * (1.0f - frac) + ref_window[idx_safe + 1] * frac;
                            }
                        }
                    }
                }
            } else if (active_analyzer && active_analyzer->has_reference()) {
                // Single bank mode - original code
                const float* ref_window = active_analyzer->get_reference_window();
                const auto& config = active_analyzer->get_config();
                int ref_size = config.correlation_window_size;
                
                // Upsample reference waveform to display size with cubic interpolation
                float scale = (float)(ref_size - 1) / (float)(DISPLAY_SAMPLES - 1);
                
                // Determine which buffer to fill based on mode
                float* target_buffer = temp_samples.data();
                if (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY) {
                    target_buffer = temp_samples2.data();
                } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY) {
                    target_buffer = temp_samples3.data();
                }
                
                // Always use stretching with interpolation for all banks
                {
                    for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                        float pos = i * scale;
                        int idx = (int)pos;
                        float frac = pos - idx;
                        
                        if (idx < ref_size - 3) {
                            // Cubic interpolation for smoother waveform
                            float y0 = (idx > 0) ? ref_window[idx - 1] : ref_window[0];
                            float y1 = ref_window[idx];
                            float y2 = ref_window[idx + 1];
                            float y3 = (idx < ref_size - 2) ? ref_window[idx + 2] : ref_window[ref_size - 1];
                            
                            float a0 = y3 - y2 - y0 + y1;
                            float a1 = y0 - y1 - a0;
                            float a2 = y2 - y0;
                            float a3 = y1;
                            
                            target_buffer[i] = a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
                        } else {
                            // Linear interpolation at edges
                            int idx_safe = std::min(idx, ref_size - 2);
                            target_buffer[i] = ref_window[idx_safe] * (1.0f - frac) + ref_window[idx_safe + 1] * frac;
                        }
                    }
                }
            }
        } else {
            // Use normal phase buffer
            if (gui_state.template_mode == GuiState::TemplateBankMode::BANK1_ONLY || 
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD ||
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
                const float* phase_buffer = phase_analyzer.get_phase_buffer();
                size_t read_pos = state.read_position;
                
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    temp_samples[i] = phase_buffer[(read_pos + i) % phase_analyzer.get_phase_buffer_size()];
                }
            }
            
            if (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY || 
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD ||
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
                const float* phase_buffer2 = phase_analyzer2.get_phase_buffer();
                size_t read_pos2 = state2.read_position;
                
                // Bank 2 has 1024 window, stretch with interpolation to fill display
                int window_size2 = 1024;
                float scale2 = (float)(window_size2 - 1) / (float)(DISPLAY_SAMPLES - 1);
                
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    float pos = i * scale2;
                    int idx = (int)pos;
                    float frac = pos - idx;
                    
                    if (idx < window_size2 - 3) {
                        // Cubic interpolation
                        float y0 = (idx > 0) ? phase_buffer2[(read_pos2 + idx - 1) % phase_analyzer2.get_phase_buffer_size()] : phase_buffer2[(read_pos2 + 0) % phase_analyzer2.get_phase_buffer_size()];
                        float y1 = phase_buffer2[(read_pos2 + idx) % phase_analyzer2.get_phase_buffer_size()];
                        float y2 = phase_buffer2[(read_pos2 + idx + 1) % phase_analyzer2.get_phase_buffer_size()];
                        float y3 = (idx < window_size2 - 2) ? phase_buffer2[(read_pos2 + idx + 2) % phase_analyzer2.get_phase_buffer_size()] : phase_buffer2[(read_pos2 + window_size2 - 1) % phase_analyzer2.get_phase_buffer_size()];
                        
                        float a0 = y3 - y2 - y0 + y1;
                        float a1 = y0 - y1 - a0;
                        float a2 = y2 - y0;
                        float a3 = y1;
                        
                        temp_samples2[i] = a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
                    } else {
                        // Linear interpolation at edges
                        int idx_safe = std::min(idx, window_size2 - 2);
                        float y0 = phase_buffer2[(read_pos2 + idx_safe) % phase_analyzer2.get_phase_buffer_size()];
                        float y1 = phase_buffer2[(read_pos2 + idx_safe + 1) % phase_analyzer2.get_phase_buffer_size()];
                        temp_samples2[i] = y0 * (1.0f - frac) + y1 * frac;
                    }
                }
                
                // Debug: Check if we got any data
                float max_val2 = 0;
                for (size_t i = 0; i < 100; ++i) {
                    max_val2 = std::max(max_val2, std::abs(temp_samples2[i]));
                }
                if (max_val2 < 0.0001f) {
                    // Try direct copy without repetition for debugging
                    for (size_t i = 0; i < std::min((size_t)DISPLAY_SAMPLES, phase_analyzer2.get_phase_buffer_size()); ++i) {
                        temp_samples2[i] = phase_buffer2[(read_pos2 + i) % phase_analyzer2.get_phase_buffer_size()];
                    }
                }
            }
            
            if (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY || 
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD ||
                gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ||
                gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
                const float* phase_buffer3 = phase_analyzer3.get_phase_buffer();
                size_t read_pos3 = state3.read_position;
                
                // Bank 3 has 512 window, stretch with interpolation to fill display
                int window_size3 = 512;
                float scale3 = (float)(window_size3 - 1) / (float)(DISPLAY_SAMPLES - 1);
                
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    float pos = i * scale3;
                    int idx = (int)pos;
                    float frac = pos - idx;
                    
                    if (idx < window_size3 - 3) {
                        // Cubic interpolation
                        float y0 = (idx > 0) ? phase_buffer3[(read_pos3 + idx - 1) % phase_analyzer3.get_phase_buffer_size()] : phase_buffer3[(read_pos3 + 0) % phase_analyzer3.get_phase_buffer_size()];
                        float y1 = phase_buffer3[(read_pos3 + idx) % phase_analyzer3.get_phase_buffer_size()];
                        float y2 = phase_buffer3[(read_pos3 + idx + 1) % phase_analyzer3.get_phase_buffer_size()];
                        float y3 = (idx < window_size3 - 2) ? phase_buffer3[(read_pos3 + idx + 2) % phase_analyzer3.get_phase_buffer_size()] : phase_buffer3[(read_pos3 + window_size3 - 1) % phase_analyzer3.get_phase_buffer_size()];
                        
                        float a0 = y3 - y2 - y0 + y1;
                        float a1 = y0 - y1 - a0;
                        float a2 = y2 - y0;
                        float a3 = y1;
                        
                        temp_samples3[i] = a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
                    } else {
                        // Linear interpolation at edges
                        int idx_safe = std::min(idx, window_size3 - 2);
                        float y0 = phase_buffer3[(read_pos3 + idx_safe) % phase_analyzer3.get_phase_buffer_size()];
                        float y1 = phase_buffer3[(read_pos3 + idx_safe + 1) % phase_analyzer3.get_phase_buffer_size()];
                        temp_samples3[i] = y0 * (1.0f - frac) + y1 * frac;
                    }
                }
            }
        }
        
        // Handle blending modes
        if (gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD) {
            // Additive blend with normalization
            float bank1_weight = state.best_correlation;
            float bank2_weight = state2.best_correlation;
            float bank3_weight = state3.best_correlation;
            float total_weight = bank1_weight + bank2_weight + bank3_weight;
            if (total_weight > 0.0f) {
                bank1_weight /= total_weight;
                bank2_weight /= total_weight;
                bank3_weight /= total_weight;
            } else {
                bank1_weight = 0.333f;
                bank2_weight = 0.333f;
                bank3_weight = 0.333f;
            }
            
            for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                temp_samples[i] = temp_samples[i] * bank1_weight + 
                                  temp_samples2[i] * bank2_weight + 
                                  temp_samples3[i] * bank3_weight;
            }
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX) {
            // Show whichever has best correlation
            if (state3.best_correlation > state2.best_correlation && 
                state3.best_correlation > state.best_correlation) {
                std::copy(temp_samples3.begin(), temp_samples3.end(), temp_samples.begin());
            } else if (state2.best_correlation > state.best_correlation) {
                std::copy(temp_samples2.begin(), temp_samples2.end(), temp_samples.begin());
            }
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED) {
            // Layer all banks - use weighted addition based on correlation
            for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                // Weight each bank by its correlation strength
                // Give each bank equal weight potential, modulated by correlation
                float w1 = state.best_correlation;
                float w2 = state2.best_correlation;
                float w3 = state3.best_correlation;
                
                // Add all banks together with equal importance
                temp_samples[i] = temp_samples[i] * w1 + 
                                  temp_samples2[i] * w2 + 
                                  temp_samples3[i] * w3;
                
                // Normalize based on total weight to prevent clipping
                float total_weight = w1 + w2 + w3;
                if (total_weight > 0.001f) {
                    temp_samples[i] /= total_weight;
                }
            }
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
            // We'll draw all 3 waveforms on top of each other with different colors
            // This requires multiple draw calls with different colors
            // For now, store a flag to handle this in the render section
            // temp_samples already has bank 1 data
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY) {
            std::copy(temp_samples2.begin(), temp_samples2.end(), temp_samples.begin());
            
            // Debug: Check what we're copying
            static int debug_counter = 0;
            if (debug_counter++ % 60 == 0) { // Every 60 frames
                float max_val = 0;
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    max_val = std::max(max_val, std::abs(temp_samples[i]));
                }
                printf("BANK2_ONLY: max value in temp_samples after copy: %f\n", max_val);
            }
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY) {
            std::copy(temp_samples3.begin(), temp_samples3.end(), temp_samples.begin());
        }
        
        // Copy samples to display buffer (already filtered if filters are enabled)
        for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
            display_buffer[i].x = temp_samples[i];
            display_buffer[i].y = 0.0f;
            display_buffer[i].z = 0.0f;
            display_buffer[i].w = 0.0f;
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
        
        // Set trigger level based on template mode
        float display_correlation = state.best_correlation;
        if (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY) {
            display_correlation = state2.best_correlation;
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY) {
            display_correlation = state3.best_correlation;
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX) {
            display_correlation = std::max({state.best_correlation, state2.best_correlation, state3.best_correlation});
        } else if (gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD || 
                   gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED) {
            display_correlation = (state.best_correlation + state2.best_correlation + state3.best_correlation) / 3.0f;
        }
        
        glUniform1f(trigger_level_loc, display_correlation);
        glUniform1i(phase_lock_enabled_loc, gui_state.phase_lock_enabled ? 1 : 0);
        glUniform1i(reference_mode_loc, gui_state.show_reference_as_main ? 1 : 0);
        
        // Ghost trails effect
        if (gui_state.ghost_trails_enabled) {
            // Clear screen first frame
            static bool first_frame = true;
            if (first_frame) {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                first_frame = false;
            }
            
            // Enable blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // Update ghost trail buffers periodically for smoother trails
            frames_since_trail_update++;
            int update_interval = 2; // Update every 2 frames
            
            if (frames_since_trail_update >= update_interval) {
                // Copy current display buffer to ghost trail
                std::memcpy(ghost_trail_buffers[ghost_trail_write_index], 
                           display_buffer, sizeof(display_buffer));
                ghost_trail_write_index = (ghost_trail_write_index + 1) % MAX_GHOST_TRAILS;
                frames_since_trail_update = 0;
            }
            
            // Clear background
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            // Draw ghost trails from oldest to newest
            int num_trails = std::min(gui_state.ghost_trail_count, MAX_GHOST_TRAILS);
            for (int i = 0; i < num_trails; i++) {
                int trail_idx = (ghost_trail_write_index - num_trails + i + MAX_GHOST_TRAILS) % MAX_GHOST_TRAILS;
                float age = (float)i / (float)num_trails;
                
                // Calculate alpha based on age and fade speed
                float alpha = age * age * (1.0f - gui_state.ghost_fade_speed * 5.0f);
                if (alpha <= 0.01f) continue; // Skip very faint trails
                
                // Upload ghost trail buffer
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                void* ghost_ptr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (ghost_ptr) {
                    memcpy(ghost_ptr, ghost_trail_buffers[trail_idx], sizeof(display_buffer));
                    glUnmapBuffer(GL_UNIFORM_BUFFER);
                }
                
                // Set uniforms for ghost trail
                glUniform1f(waveform_alpha_loc, alpha);
                
                // Color gradient from purple to cyan based on age
                float r = 0.5f * (1.0f - age);
                float g = 0.7f * age;
                float b = 0.9f;
                glUniform3f(waveform_color_loc, r, g, b);
                
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // Draw current waveform on top
            glBindBuffer(GL_UNIFORM_BUFFER, ubo);
            void* ptr2 = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if (ptr2) {
                memcpy(ptr2, display_buffer, sizeof(display_buffer));
                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
            glUniform1f(waveform_alpha_loc, 1.0f);
            glUniform3f(waveform_color_loc, 0.0f, 1.0f, 0.9f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            
            glDisable(GL_BLEND);
        } else {
            static bool first_frame = false;
            first_frame = true;
            // Normal rendering - clear and draw
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            if (gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) {
                // Draw all 3 waveforms with different colors
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bank 1 - Cyan/Green theme (current default)
                glUniform1f(waveform_alpha_loc, 0.8f);
                glUniform3f(waveform_color_loc, 0.0f, 1.0f, 0.9f);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                
                // Bank 2 - Purple/Magenta theme
                // Copy bank 2 data to display buffer
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    display_buffer[i].x = temp_samples2[i];
                }
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                void* ptr2 = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (ptr2) {
                    memcpy(ptr2, display_buffer, sizeof(display_buffer));
                    glUnmapBuffer(GL_UNIFORM_BUFFER);
                }
                glUniform3f(waveform_color_loc, 1.0f, 0.0f, 1.0f);  // Magenta base
                glDrawArrays(GL_TRIANGLES, 0, 6);
                
                // Bank 3 - Orange/Yellow theme
                // Copy bank 3 data to display buffer
                for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
                    display_buffer[i].x = temp_samples3[i];
                }
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                void* ptr3 = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (ptr3) {
                    memcpy(ptr3, display_buffer, sizeof(display_buffer));
                    glUnmapBuffer(GL_UNIFORM_BUFFER);
                }
                glUniform3f(waveform_color_loc, 1.0f, 0.5f, 0.0f);  // Orange base
                glDrawArrays(GL_TRIANGLES, 0, 6);
                
                glDisable(GL_BLEND);
            } else {
                // Single waveform - normal rendering
                glUniform1f(waveform_alpha_loc, 1.0f);
                glUniform3f(waveform_color_loc, 0.0f, 1.0f, 0.9f);  // Default color
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
        
        // ImGui rendering
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
            
            ImGui::Begin("Audio Visualizer Controls");
            
            ImGui::Text("FPS: %.1f", current_fps);
            ImGui::Text("Audio Buffer: %zu samples", audio_buffer_size);
            ImGui::Text("Latency: ~%.1f ms", audio_buffer_size / 44.1);
            
            ImGui::Separator();
            
            // Window controls
            if (ImGui::Button(window_state.is_fullscreen ? "Exit Fullscreen (F11)" : "Fullscreen (F11)")) {
                if (!window_state.is_fullscreen) {
                    // Save current window position and size
                    glfwGetWindowPos(window, &window_state.x, &window_state.y);
                    glfwGetWindowSize(window, &window_state.width, &window_state.height);
                    
                    // Get primary monitor and its video mode
                    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                    
                    // Set windowed fullscreen
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    window_state.is_fullscreen = true;
                } else {
                    // Restore windowed mode
                    glfwSetWindowMonitor(window, nullptr, window_state.x, window_state.y, 
                                       window_state.width, window_state.height, 0);
                    window_state.is_fullscreen = false;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle fullscreen mode (Alt+Enter also works)");
            }
            
            ImGui::Separator();
            
            // Phase Lock Settings
            if (ImGui::CollapsingHeader("Phase Lock Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Phase Lock", &gui_state.phase_lock_enabled);
                
                if (gui_state.phase_lock_enabled) {
                    // Template bank selection
                    ImGui::Separator();
                    ImGui::Text("Template Banks:");
                    
                    const char* template_modes[] = { "Bank 1 Only", "Bank 2 Only", "Bank 3 Only", "All Layered", "All Separate", "Blend (Add)", "Blend (Max)" };
                    int current_mode = (int)gui_state.template_mode;
                    if (ImGui::Combo("Display Mode", &current_mode, template_modes, IM_ARRAYSIZE(template_modes))) {
                        gui_state.template_mode = (GuiState::TemplateBankMode)current_mode;
                    }
                    
                    // Show correlation status for all banks
                    ImGui::Text("Bank 1 (40Hz+) - Correlation: %.2f %s", state.best_correlation, 
                               state.has_lock ? "[LOCKED]" : "[SEARCHING]");
                    ImGui::Text("Bank 2 (160Hz+) - Correlation: %.2f %s", state2.best_correlation,
                               state2.has_lock ? "[LOCKED]" : "[SEARCHING]");
                    ImGui::Text("Bank 3 (640Hz+) - Correlation: %.2f %s", state3.best_correlation,
                               state3.has_lock ? "[LOCKED]" : "[SEARCHING]");
                    
                    ImGui::Separator();
                    ImGui::Text("Bank 1 Settings (40Hz+ - Bass):");
                    
                    auto config = phase_analyzer.get_config();
                    
                    if (ImGui::SliderFloat("Phase Smoothing", &config.phase_smoothing, 0.0f, 0.99f)) {
                        phase_analyzer.set_config(config);
                    }
                    if (ImGui::SliderFloat("Correlation Threshold", &config.correlation_threshold, 0.001f, 0.99f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                        phase_analyzer.set_config(config);
                    }
                    if (ImGui::SliderInt("Correlation Window", &config.correlation_window_size, 128, 1024)) {
                        phase_analyzer.set_config(config);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Reference Mode:");
                    
                    bool is_accumulator = config.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                    if (ImGui::RadioButton("Accumulator", is_accumulator)) {
                        config.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                        phase_analyzer.set_config(config);
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("EMA", !is_accumulator)) {
                        config.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;
                        phase_analyzer.set_config(config);
                    }
                    
                    if (config.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR) {
                        if (ImGui::SliderInt("Reset After", &config.accumulator_reset_count, 10, 200)) {
                            phase_analyzer.set_config(config);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Number of matches before resetting accumulator");
                        }
                    } else {
                        float ema_alpha = phase_analyzer.get_ema_alpha();
                        if (ImGui::SliderFloat("EMA Alpha", &ema_alpha, 0.001f, 0.999f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                            phase_analyzer.set_ema_alpha(ema_alpha);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Lower = more stable reference, Higher = faster adaptation");
                        }
                    }
                    
                    ImGui::Separator();
                    
                    // Frequency filter settings
                    if (ImGui::Checkbox("Use Frequency Filter", &config.use_frequency_filter)) {
                        phase_analyzer.set_config(config);
                    }
                    
                    if (config.use_frequency_filter) {
                        if (ImGui::SliderFloat("Low Freq (Hz)", &config.filter_low_frequency, 20.0f, 2000.0f)) {
                            phase_analyzer.set_config(config);
                        }
                        if (ImGui::SliderFloat("High Freq (Hz)", &config.filter_high_frequency, 
                                              config.filter_low_frequency + 50.0f, 10000.0f)) {
                            phase_analyzer.set_config(config);
                        }
                        
                        ImGui::Text("Band: %.0f - %.0f Hz", config.filter_low_frequency, config.filter_high_frequency);
                    }
                    
                    if (ImGui::Button("Reset Bank 1 Reference")) {
                        phase_analyzer.reset();
                    }
                    
                    ImGui::Separator();
                    
                    // Bank 2 controls
                    ImGui::Checkbox("Show Bank 2 Controls", &gui_state.show_bank2_controls);
                    
                    if (gui_state.show_bank2_controls) {
                        ImGui::Separator();
                        ImGui::Text("Bank 2 Settings (160Hz+ - Mids):");
                        
                        auto config2 = phase_analyzer2.get_config();
                        
                        if (ImGui::SliderFloat("Phase Smoothing##2", &config2.phase_smoothing, 0.0f, 0.99f)) {
                            phase_analyzer2.set_config(config2);
                        }
                        if (ImGui::SliderFloat("Correlation Threshold##2", &config2.correlation_threshold, 0.001f, 0.99f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                            phase_analyzer2.set_config(config2);
                        }
                        if (ImGui::SliderInt("Correlation Window##2", &config2.correlation_window_size, 128, 1024)) {
                            phase_analyzer2.set_config(config2);
                        }
                        
                        ImGui::Text("Reference Mode:");
                        bool is_accumulator2 = config2.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                        if (ImGui::RadioButton("Accumulator##2", is_accumulator2)) {
                            config2.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                            phase_analyzer2.set_config(config2);
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("EMA##2", !is_accumulator2)) {
                            config2.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;
                            phase_analyzer2.set_config(config2);
                        }
                        
                        if (config2.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::EMA) {
                            float ema_alpha2 = phase_analyzer2.get_ema_alpha();
                            if (ImGui::SliderFloat("EMA Alpha##2", &ema_alpha2, 0.001f, 0.999f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                                phase_analyzer2.set_ema_alpha(ema_alpha2);
                            }
                        }
                        
                        if (ImGui::Button("Reset Bank 2 Reference")) {
                            phase_analyzer2.reset();
                        }
                    }
                    
                    // Bank 3 controls
                    ImGui::Checkbox("Show Bank 3 Controls", &gui_state.show_bank3_controls);
                    
                    if (gui_state.show_bank3_controls) {
                        ImGui::Separator();
                        ImGui::Text("Bank 3 Settings (640Hz+ - Upper Mids/Highs):");
                        
                        auto config3 = phase_analyzer3.get_config();
                        
                        if (ImGui::SliderFloat("Phase Smoothing##3", &config3.phase_smoothing, 0.0f, 0.99f)) {
                            phase_analyzer3.set_config(config3);
                        }
                        if (ImGui::SliderFloat("Correlation Threshold##3", &config3.correlation_threshold, 0.001f, 0.99f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                            phase_analyzer3.set_config(config3);
                        }
                        if (ImGui::SliderInt("Correlation Window##3", &config3.correlation_window_size, 128, 1024)) {
                            phase_analyzer3.set_config(config3);
                        }
                        
                        ImGui::Text("Reference Mode:");
                        bool is_accumulator3 = config3.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                        if (ImGui::RadioButton("Accumulator##3", is_accumulator3)) {
                            config3.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::ACCUMULATOR;
                            phase_analyzer3.set_config(config3);
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("EMA##3", !is_accumulator3)) {
                            config3.reference_mode = visualization::PhaseLockAnalyzer::ReferenceMode::EMA;
                            phase_analyzer3.set_config(config3);
                        }
                        
                        if (config3.reference_mode == visualization::PhaseLockAnalyzer::ReferenceMode::EMA) {
                            float ema_alpha3 = phase_analyzer3.get_ema_alpha();
                            if (ImGui::SliderFloat("EMA Alpha##3", &ema_alpha3, 0.001f, 0.999f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                                phase_analyzer3.set_ema_alpha(ema_alpha3);
                            }
                        }
                        
                        if (ImGui::Button("Reset Bank 3 Reference")) {
                            phase_analyzer3.reset();
                        }
                    }
                    
                    ImGui::Separator();
                    
                    // Option to show reference waveform as main display
                    if ((gui_state.template_mode == GuiState::TemplateBankMode::BANK1_ONLY && phase_analyzer.has_reference()) ||
                        (gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY && phase_analyzer2.has_reference()) ||
                        (gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY && phase_analyzer3.has_reference()) ||
                        ((gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD || 
                          gui_state.template_mode == GuiState::TemplateBankMode::BLEND_MAX ||
                          gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ||
                          gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE) && 
                         (phase_analyzer.has_reference() || phase_analyzer2.has_reference() || phase_analyzer3.has_reference()))) {
                        if (ImGui::Checkbox("Show Reference as Main", &gui_state.show_reference_as_main)) {
                            // Nothing extra needed, will be handled in render loop
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Display the upsampled reference waveform with better antialiasing");
                        }
                        
                        if (gui_state.show_reference_as_main) {
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), 
                                             "Displaying reference waveform (%d → %d samples)",
                                             config.correlation_window_size, DISPLAY_SAMPLES);
                            ImGui::Text("Using cubic interpolation for smooth rendering");
                        }
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
                
                ImGui::Separator();
                ImGui::Text("Ghost Trails");
                ImGui::Checkbox("Enable Ghost Trails", &gui_state.ghost_trails_enabled);
                
                if (gui_state.ghost_trails_enabled) {
                    ImGui::SliderFloat("Fade Speed", &gui_state.ghost_fade_speed, 0.001f, 0.1f, "%.3f");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Lower = longer trails, Higher = faster fade");
                    }
                    
                    // Show dynamic fade info when phase lock is enabled
                    if (gui_state.phase_lock_enabled) {
                        float correlation_factor = state.best_correlation;
                        float dynamic_fade = gui_state.ghost_fade_speed * (2.0f - correlation_factor * 1.5f);
                        ImGui::Text("Dynamic fade rate: %.3f", dynamic_fade);
                        ImGui::Text("(Based on correlation: %.2f)", correlation_factor);
                        
                        // Visual indicator bar
                        ImGui::ProgressBar(1.0f - dynamic_fade * 10.0f, ImVec2(-1, 0), "Trail Persistence");
                    }
                }
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Show Demo Window", &gui_state.show_demo_window);
            ImGui::Checkbox("Show Correlation Graph", &gui_state.show_correlation_graph);
            ImGui::Checkbox("Show Reference Waveform", &gui_state.show_reference_waveform);
            ImGui::Checkbox("Show Spectrum Analyzer", &gui_state.show_spectrum_analyzer);
            ImGui::Checkbox("Show Frequency Peaks", &gui_state.show_frequency_peaks);
            ImGui::Checkbox("Show Filtered Waveform", &gui_state.show_filtered_waveform);
            ImGui::Checkbox("Show Simple Filters", &gui_state.show_simple_filters);
            
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
            
            // Reference waveform window
            if (gui_state.show_reference_waveform && gui_state.phase_lock_enabled) {
                ImGui::SetNextWindowPos(ImVec2(370, 220), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Reference Waveforms");
                
                // Bank 1
                if (phase_analyzer.has_reference()) {
                    ImGui::Text("Bank 1 (40Hz+ - Bass):");
                    const float* ref_window = phase_analyzer.get_reference_window();
                    const auto& config = phase_analyzer.get_config();
                    
                    // Create a vector for ImGui plotting
                    std::vector<float> ref_samples(ref_window, ref_window + config.correlation_window_size);
                    
                    ImGui::PlotLines("Reference 1", ref_samples.data(), ref_samples.size(),
                                     0, nullptr, -1.0f, 1.0f, ImVec2(0, 100));
                    
                    ImGui::Text("Window Size: %d samples, Matches: %d", 
                               config.correlation_window_size, phase_analyzer.get_reference_count());
                    ImGui::Text("Correlation: %.3f", state.best_correlation);
                }
                
                ImGui::Separator();
                
                // Bank 2
                if (phase_analyzer2.has_reference()) {
                    ImGui::Text("Bank 2 (160Hz+ - Mids):");
                    const float* ref_window2 = phase_analyzer2.get_reference_window();
                    const auto& config2 = phase_analyzer2.get_config();
                    
                    std::vector<float> ref_samples2(ref_window2, ref_window2 + config2.correlation_window_size);
                    
                    ImGui::PlotLines("Reference 2", ref_samples2.data(), ref_samples2.size(),
                                     0, nullptr, -1.0f, 1.0f, ImVec2(0, 100));
                    
                    ImGui::Text("Window Size: %d samples, Matches: %d", 
                               config2.correlation_window_size, phase_analyzer2.get_reference_count());
                    ImGui::Text("Correlation: %.3f", state2.best_correlation);
                }
                
                ImGui::Separator();
                
                // Bank 3
                if (phase_analyzer3.has_reference()) {
                    ImGui::Text("Bank 3 (640Hz+ - Upper Mids/Highs):");
                    const float* ref_window3 = phase_analyzer3.get_reference_window();
                    const auto& config3 = phase_analyzer3.get_config();
                    
                    std::vector<float> ref_samples3(ref_window3, ref_window3 + config3.correlation_window_size);
                    
                    ImGui::PlotLines("Reference 3", ref_samples3.data(), ref_samples3.size(),
                                     0, nullptr, -1.0f, 1.0f, ImVec2(0, 100));
                    
                    ImGui::Text("Window Size: %d samples, Matches: %d", 
                               config3.correlation_window_size, phase_analyzer3.get_reference_count());
                    ImGui::Text("Correlation: %.3f", state3.best_correlation);
                }
                
                ImGui::Separator();
                
                // Show actual display buffers for debugging
                ImGui::Text("Display Buffers (first 100 samples):");
                ImGui::Text("Current mode: %s", 
                           gui_state.template_mode == GuiState::TemplateBankMode::BANK1_ONLY ? "Bank 1 Only" :
                           gui_state.template_mode == GuiState::TemplateBankMode::BANK2_ONLY ? "Bank 2 Only" :
                           gui_state.template_mode == GuiState::TemplateBankMode::BANK3_ONLY ? "Bank 3 Only" :
                           gui_state.template_mode == GuiState::TemplateBankMode::ALL_LAYERED ? "All Layered" :
                           gui_state.template_mode == GuiState::TemplateBankMode::ALL_SEPARATE ? "All Separate" :
                           gui_state.template_mode == GuiState::TemplateBankMode::BLEND_ADD ? "Blend Add" : "Blend Max");
                
                std::vector<float> display_debug1(temp_samples.begin(), temp_samples.begin() + std::min((size_t)100, temp_samples.size()));
                std::vector<float> display_debug2(temp_samples2.begin(), temp_samples2.begin() + std::min((size_t)100, temp_samples2.size()));
                std::vector<float> display_debug3(temp_samples3.begin(), temp_samples3.begin() + std::min((size_t)100, temp_samples3.size()));
                
                // Calculate RMS for each bank to see if there's signal
                float rms1 = 0, rms2 = 0, rms3 = 0;
                for (size_t i = 0; i < 100 && i < temp_samples.size(); ++i) {
                    rms1 += temp_samples[i] * temp_samples[i];
                    rms2 += temp_samples2[i] * temp_samples2[i];
                    rms3 += temp_samples3[i] * temp_samples3[i];
                }
                rms1 = sqrt(rms1 / 100);
                rms2 = sqrt(rms2 / 100);
                rms3 = sqrt(rms3 / 100);
                
                ImGui::Text("RMS levels: Bank1=%.4f, Bank2=%.4f, Bank3=%.4f", rms1, rms2, rms3);
                
                // Check phase buffer contents directly
                const float* pb1 = phase_analyzer.get_phase_buffer();
                const float* pb2 = phase_analyzer2.get_phase_buffer();
                const float* pb3 = phase_analyzer3.get_phase_buffer();
                
                float pb_rms1 = 0, pb_rms2 = 0, pb_rms3 = 0;
                for (size_t i = 0; i < 100; ++i) {
                    pb_rms1 += pb1[i] * pb1[i];
                    pb_rms2 += pb2[i] * pb2[i];
                    pb_rms3 += pb3[i] * pb3[i];
                }
                pb_rms1 = sqrt(pb_rms1 / 100);
                pb_rms2 = sqrt(pb_rms2 / 100);
                pb_rms3 = sqrt(pb_rms3 / 100);
                
                ImGui::Text("Phase Buffer RMS: Bank1=%.4f, Bank2=%.4f, Bank3=%.4f", pb_rms1, pb_rms2, pb_rms3);
                ImGui::Text("Read positions: Bank1=%zu, Bank2=%zu, Bank3=%zu", 
                           state.read_position, state2.read_position, state3.read_position);
                
                ImGui::PlotLines("Display 1", display_debug1.data(), display_debug1.size(),
                                 0, nullptr, -1.0f, 1.0f, ImVec2(0, 50));
                ImGui::PlotLines("Display 2", display_debug2.data(), display_debug2.size(),
                                 0, nullptr, -1.0f, 1.0f, ImVec2(0, 50));
                ImGui::PlotLines("Display 3", display_debug3.data(), display_debug3.size(),
                                 0, nullptr, -1.0f, 1.0f, ImVec2(0, 50));
                
                ImGui::End();
            }
            
            // Filtered waveform window
            if (gui_state.show_filtered_waveform && gui_state.phase_lock_enabled) {
                auto phase_config = phase_analyzer.get_config();
                if (phase_config.use_frequency_filter && phase_analyzer.get_filtered_buffer()) {
                    ImGui::SetNextWindowPos(ImVec2(370, 430), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
                    
                    ImGui::Begin("Filtered Waveform");
                    
                    const float* filtered_buffer = phase_analyzer.get_filtered_buffer();
                    size_t buffer_size = phase_analyzer.get_filtered_buffer_size();
                    
                    // Display the same region as the main waveform for comparison
                    auto state = phase_analyzer.analyze(gui_state.phase_lock_enabled);
                    size_t display_window = std::min((size_t)1024, buffer_size);
                    size_t start_pos = state.read_position;
                    
                    std::vector<float> filtered_display(display_window);
                    for (size_t i = 0; i < display_window; ++i) {
                        filtered_display[i] = filtered_buffer[(start_pos + i) % buffer_size];
                    }
                    
                    ImGui::PlotLines("Filtered Signal", filtered_display.data(), filtered_display.size(),
                                     0, nullptr, -1.0f, 1.0f, ImVec2(0, 150));
                    
                    ImGui::Text("Frequency Band: %.0f - %.0f Hz", 
                               phase_config.filter_low_frequency, phase_config.filter_high_frequency);
                    
                    ImGui::End();
                }
            }
            
            // Spectrum Analyzer window
            if (gui_state.show_spectrum_analyzer) {
                ImGui::SetNextWindowPos(ImVec2(780, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Spectrum Analyzer");
                
                const auto& freq_state = freq_analyzer.get_state();
                const auto& spectrum = freq_state.magnitude_spectrum;
                
                if (!spectrum.empty()) {
                    // Only display up to Nyquist frequency (half the spectrum)
                    size_t display_bins = spectrum.size() / 2;
                    
                    // Create log scale display
                    std::vector<float> log_spectrum(display_bins);
                    for (size_t i = 0; i < display_bins; ++i) {
                        // Convert to dB with -60dB floor
                        float mag = spectrum[i];
                        float db = 20.0f * std::log10(std::max(mag, 0.001f));
                        log_spectrum[i] = std::max(db, -60.0f);
                    }
                    
                    ImGui::PlotLines("Spectrum (dB)", log_spectrum.data(), log_spectrum.size(),
                                     0, nullptr, -60.0f, 0.0f, ImVec2(0, 200));
                    
                    ImGui::Text("Total Energy: %.3f", freq_state.total_energy);
                    ImGui::Text("Dominant Frequency: %.1f Hz", freq_state.dominant_frequency);
                }
                
                ImGui::Separator();
                
                // FFT settings
                if (ImGui::CollapsingHeader("FFT Settings")) {
                    auto freq_config = freq_analyzer.get_config();
                    
                    int fft_size = (int)freq_config.fft_size;
                    if (ImGui::Combo("FFT Size", &fft_size, "512\0001024\0002048\0004096\0008192\000")) {
                        freq_config.fft_size = (size_t)std::pow(2, 9 + fft_size);
                        freq_analyzer.set_config(freq_config);
                    }
                    
                    if (ImGui::SliderFloat("Peak Threshold", &freq_config.peak_threshold, 0.01f, 0.5f)) {
                        freq_analyzer.set_config(freq_config);
                    }
                    
                    if (ImGui::SliderInt("Max Peaks", (int*)&freq_config.max_peaks, 1, 10)) {
                        freq_analyzer.set_config(freq_config);
                    }
                }
                
                ImGui::End();
            }
            
            // Frequency Peaks window
            if (gui_state.show_frequency_peaks) {
                ImGui::SetNextWindowPos(ImVec2(780, 320), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Frequency Peaks");
                
                const auto& freq_state = freq_analyzer.get_state();
                const auto& peaks = freq_state.peaks;
                
                if (!peaks.empty()) {
                    ImGui::Columns(3, "PeaksColumns");
                    ImGui::SetColumnWidth(0, 100);
                    ImGui::SetColumnWidth(1, 150);
                    ImGui::SetColumnWidth(2, 150);
                    
                    ImGui::Text("Rank");
                    ImGui::NextColumn();
                    ImGui::Text("Frequency (Hz)");
                    ImGui::NextColumn();
                    ImGui::Text("Magnitude");
                    ImGui::NextColumn();
                    ImGui::Separator();
                    
                    for (size_t i = 0; i < peaks.size(); ++i) {
                        ImGui::Text("#%zu", i + 1);
                        ImGui::NextColumn();
                        ImGui::Text("%.1f", peaks[i].frequency);
                        ImGui::NextColumn();
                        ImGui::Text("%.4f", peaks[i].magnitude);
                        ImGui::NextColumn();
                    }
                    
                    ImGui::Columns(1);
                } else {
                    ImGui::Text("No peaks detected");
                }
                
                ImGui::Separator();
                
                // Show peak history as a graph
                const auto& peak_history = freq_analyzer.get_peak_history();
                if (!peak_history.empty() && !peak_history.back().empty()) {
                    ImGui::Text("Dominant Frequency History:");
                    
                    std::vector<float> freq_history;
                    freq_history.reserve(peak_history.size());
                    
                    for (const auto& peaks_frame : peak_history) {
                        if (!peaks_frame.empty()) {
                            freq_history.push_back(peaks_frame[0].frequency);
                        } else {
                            freq_history.push_back(0.0f);
                        }
                    }
                    
                    ImGui::PlotLines("Frequency (Hz)", freq_history.data(), freq_history.size(),
                                     0, nullptr, 0.0f, 2000.0f, ImVec2(0, 80));
                }
                
                ImGui::End();
            }
            
            // Simple Filters window
            if (gui_state.show_simple_filters) {
                ImGui::SetNextWindowPos(ImVec2(1290, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 450), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Simple Filters");
                
                auto filter_config = simple_filters.get_config();
                bool config_changed = false;
                
                // Info text
                ImGui::TextWrapped("Filters affect both visualization and phase locking");
                ImGui::Separator();
                
                // High-pass filter section
                if (ImGui::CollapsingHeader("High-Pass Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::Checkbox("Enable High-Pass", &filter_config.highpass_enabled)) {
                        config_changed = true;
                    }
                    
                    if (filter_config.highpass_enabled) {
                        if (ImGui::SliderFloat("HP Cutoff (Hz)", &filter_config.highpass_cutoff, 
                                              20.0f, 2000.0f, "%.0f", ImGuiSliderFlags_Logarithmic)) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Removes frequencies below this value");
                        }
                        
                        if (ImGui::SliderFloat("HP Resonance", &filter_config.highpass_resonance, 
                                              0.5f, 2.0f, "%.2f")) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Q factor - higher values create sharper cutoff");
                        }
                    }
                }
                
                ImGui::Separator();
                
                // Low-pass filter section
                if (ImGui::CollapsingHeader("Low-Pass Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::Checkbox("Enable Low-Pass", &filter_config.lowpass_enabled)) {
                        config_changed = true;
                    }
                    
                    if (filter_config.lowpass_enabled) {
                        if (ImGui::SliderFloat("LP Cutoff (Hz)", &filter_config.lowpass_cutoff, 
                                              200.0f, 20000.0f, "%.0f", ImGuiSliderFlags_Logarithmic)) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Removes frequencies above this value");
                        }
                        
                        if (ImGui::SliderFloat("LP Resonance", &filter_config.lowpass_resonance, 
                                              0.5f, 2.0f, "%.2f")) {
                            config_changed = true;
                        }
                    }
                }
                
                ImGui::Separator();
                
                // De-esser section
                if (ImGui::CollapsingHeader("De-Esser", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::Checkbox("Enable De-Esser", &filter_config.deesser_enabled)) {
                        config_changed = true;
                    }
                    
                    if (filter_config.deesser_enabled) {
                        if (ImGui::SliderFloat("Center Freq (Hz)", &filter_config.deesser_frequency, 
                                              2000.0f, 10000.0f, "%.0f")) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Center frequency for sibilance detection");
                        }
                        
                        if (ImGui::SliderFloat("Bandwidth (Hz)", &filter_config.deesser_bandwidth, 
                                              500.0f, 4000.0f, "%.0f")) {
                            config_changed = true;
                        }
                        
                        if (ImGui::SliderFloat("Threshold", &filter_config.deesser_threshold, 
                                              0.1f, 0.9f, "%.2f")) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Level at which de-essing begins");
                        }
                        
                        if (ImGui::SliderFloat("Reduction", &filter_config.deesser_ratio, 
                                              0.0f, 1.0f, "%.2f")) {
                            config_changed = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Amount of sibilance reduction (0=none, 1=full)");
                        }
                        
                        // Show de-esser activity
                        float envelope = simple_filters.get_deesser_envelope();
                        ImGui::Text("Sibilance Level:");
                        ImGui::ProgressBar(envelope, ImVec2(-1, 0));
                    }
                }
                
                ImGui::Separator();
                
                // Filter combination info
                if ((filter_config.highpass_enabled ? 1 : 0) + 
                    (filter_config.lowpass_enabled ? 1 : 0) + 
                    (filter_config.deesser_enabled ? 1 : 0) > 0) {
                    ImGui::Text("Active filters:");
                    if (filter_config.highpass_enabled) {
                        ImGui::BulletText("High-pass: %.0f Hz", filter_config.highpass_cutoff);
                    }
                    if (filter_config.lowpass_enabled) {
                        ImGui::BulletText("Low-pass: %.0f Hz", filter_config.lowpass_cutoff);
                    }
                    if (filter_config.deesser_enabled) {
                        ImGui::BulletText("De-esser: %.0f Hz", filter_config.deesser_frequency);
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No filters active");
                }
                
                ImGui::Separator();
                
                if (ImGui::Button("Reset All Filters")) {
                    filter_config = visualization::SimpleFilters::Config();
                    filter_config.sample_rate = 44100.0f;
                    config_changed = true;
                    simple_filters.reset();
                }
                
                if (config_changed) {
                    simple_filters.set_config(filter_config);
                    // Reset phase analyzer since filtered signal has changed
                    phase_analyzer.reset();
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