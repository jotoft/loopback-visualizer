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
#include "visualization/frequency_analyzer.h"

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
    bool show_reference_waveform = true;
    bool show_spectrum_analyzer = true;
    bool show_frequency_peaks = true;
    bool show_filtered_waveform = true;
    
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
    auto soundwave_result = load_file("soundwave_optimized.glsl");
    auto vertex_result = load_file("basic_vertex.glsl");

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
    
    // Create frequency analyzer
    visualization::FrequencyAnalyzer::Config freq_config;
    freq_config.fft_size = 2048;
    freq_config.sample_rate = 44100.0f;  // Assuming 44.1kHz
    freq_config.history_size = 100;
    freq_config.peak_threshold = 0.05f;
    freq_config.max_peaks = 5;
    
    visualization::FrequencyAnalyzer freq_analyzer(freq_config);
    
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
            freq_analyzer.process_samples(audio_read_buffer, samples_read);
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
            ImGui::Checkbox("Show Reference Waveform", &gui_state.show_reference_waveform);
            ImGui::Checkbox("Show Spectrum Analyzer", &gui_state.show_spectrum_analyzer);
            ImGui::Checkbox("Show Frequency Peaks", &gui_state.show_frequency_peaks);
            ImGui::Checkbox("Show Filtered Waveform", &gui_state.show_filtered_waveform);
            
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
            if (gui_state.show_reference_waveform && gui_state.phase_lock_enabled && phase_analyzer.has_reference()) {
                ImGui::SetNextWindowPos(ImVec2(370, 220), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
                
                ImGui::Begin("Reference Waveform");
                
                const float* ref_window = phase_analyzer.get_reference_window();
                const auto& config = phase_analyzer.get_config();
                
                // Create a vector for ImGui plotting
                std::vector<float> ref_samples(ref_window, ref_window + config.correlation_window_size);
                
                ImGui::PlotLines("Reference", ref_samples.data(), ref_samples.size(),
                                 0, nullptr, -1.0f, 1.0f, ImVec2(0, 150));
                
                ImGui::Text("Window Size: %d samples", config.correlation_window_size);
                ImGui::Text("Updates every ~42ms (10 frames)");
                
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
                    
                    // Display a window of the filtered signal
                    size_t display_window = std::min((size_t)1024, buffer_size);
                    size_t start_pos = (phase_analyzer.get_phase_buffer_size() + 
                                       phase_analyzer.get_phase_buffer_size() - display_window) % 
                                       phase_analyzer.get_phase_buffer_size();
                    
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