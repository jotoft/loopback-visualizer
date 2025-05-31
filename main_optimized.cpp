#include <iostream>
#include <audio_loopback/loopback_recorder.h>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/audio_capture.h>
#include <audio_filters/filters.h>
#include <chrono>
#include <thread>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <limits>
#include <complex>
#include <cstring>
#include <iomanip>
#include "core/result.h"
#include "core/option.h"
#include "core/unit.h"

const uint32_t width = 2400;

class Initializer
{
public:
    Initializer()
    {
      std::ios_base::sync_with_stdio(false);
    }
};

struct vec4 {
    float x;
    float y; 
    float z;
    float w;
};

// Much smaller display buffer - we only need what's visible
const uint32_t DISPLAY_SAMPLES = width;
static vec4 display_buffer[DISPLAY_SAMPLES];

// Pre-allocated buffers to avoid allocations in render loop
static float audio_read_buffer[1024];  // Big enough for burst reads

// Global phase lock state for callback access
static bool phase_lock_enabled = false;  // Start with phase lock OFF for lowest latency

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


// App state for passing to callbacks
struct AppState {
    bool capture_input = false;  // false = loopback, true = input
    std::unique_ptr<audio::AudioCapture>* audio_capture_ptr = nullptr;
};

int main()
{
    Initializer _init;
    AppState app_state;
    
    // Audio capture pointer (we'll recreate when switching sources)
    std::unique_ptr<audio::AudioCapture> audio_capture;
    app_state.audio_capture_ptr = &audio_capture;
    
    // Function to switch audio source
    auto switch_audio_source = [&app_state, &audio_capture]() {
        // Stop current capture if running
        if (audio_capture) {
            audio_capture->stop();
            audio_capture.reset();
        }
        
        // Toggle source
        app_state.capture_input = !app_state.capture_input;
        
        // Get appropriate audio device
        auto device_opt = audio::get_default_sink(app_state.capture_input);
        if (device_opt.is_none()) {
            std::cerr << "No default " << (app_state.capture_input ? "input" : "sink") << " found" << std::endl;
            return false;
        }
        
        auto device = device_opt.unwrap();
        std::cout << "\nSwitching to: " << (app_state.capture_input ? "INPUT (Microphone)" : "LOOPBACK (System Audio)") << std::endl;
        std::cout << "Device: " << device.name << std::endl;
        
        // Create new capture
        audio_capture = audio::create_audio_capture(device);
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
    app_state.capture_input = false;  // Reset since switch_audio_source toggled it
    // Window data structure for callbacks (declare here, set up after window creation)
    struct WindowData {
        AppState* state;
        std::function<bool()> switch_func;
    } window_data = { &app_state, switch_audio_source };

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

    GLFWwindow *window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(width, 800, "Audio Visualizer - I: input | P: phase lock", NULL, NULL);
    
    // Get actual framebuffer size (for high DPI displays)
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    std::cout << "Window size: " << width << "x800, Framebuffer: " << fb_width << "x" << fb_height << std::endl;
    if (!window) {
        std::cout << "error 1";
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    
    // Set up window callbacks
    glfwSetWindowUserPointer(window, &window_data);
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS) {
            auto* data = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));
            
            switch (key) {
                case GLFW_KEY_I:  // 'I' for Input/Loopback toggle
                case GLFW_KEY_SPACE:  // Space bar as alternative
                    data->switch_func();
                    break;
                case GLFW_KEY_P:  // 'P' for Phase lock toggle
                    phase_lock_enabled = !phase_lock_enabled;
                    std::cout << "Phase lock: " << (phase_lock_enabled ? "ENABLED" : "DISABLED") << std::endl;
                    break;
                case GLFW_KEY_ESCAPE:
                case GLFW_KEY_Q:
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                    break;
            }
        }
    });

    // Create vertex data for fullscreen quad
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
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

    // Shader setup
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
    
    // Set resolution uniform
    GLint resolution_loc = glGetUniformLocation(shaderProgram, "resolution");
    glUniform2f(resolution_loc, (float)fb_width, (float)fb_height);

    // Create smaller UBO for just display samples
    unsigned int ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(display_buffer), nullptr, GL_DYNAMIC_DRAW);
    
    unsigned int block_index = glGetUniformBlockIndex(shaderProgram, "SamplesBlock");
    GLuint binding_point_index = 2;
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point_index, ubo);
    glUniformBlockBinding(shaderProgram, block_index, binding_point_index);
    
    GLint sample_loc = glGetUniformLocation(shaderProgram, "current_sample");
    GLint trigger_level_loc = glGetUniformLocation(shaderProgram, "trigger_level");
    GLint phase_lock_enabled_loc = glGetUniformLocation(shaderProgram, "phase_lock_enabled");

    // Timing
    glfwSwapInterval(0); // Disable VSync for minimum latency
    
    // For FPS calculation
    double last_fps_time = glfwGetTime();
    int fps_frame_count = 0;
    
    // Pre-allocate circular buffer for phase tracking
    const size_t PHASE_BUFFER_SIZE = 4096;  // Larger for correlation
    float phase_buffer[PHASE_BUFFER_SIZE] = {0}; // Initialize to zero
    size_t phase_write_pos = 0;
    
    // Cross-correlation based phase locking
    const size_t CORR_WINDOW = 512;  // Size of correlation window
    float reference_window[CORR_WINDOW] = {0};  // Reference signal for correlation
    bool has_reference = false;
    size_t phase_offset = 0;  // Current phase offset
    size_t target_phase_offset = 0;  // Target phase offset from correlation
    float phase_smoothing = 0.9f;  // Smoothing factor (0-1, higher = smoother)
    float correlation_threshold = 0.7f;  // Minimum correlation for good lock
    float best_correlation = 0.0f;
    int frames_since_reference = 0;
    
    // Initialize display buffer to zero to avoid garbage
    std::memset(display_buffer, 0, sizeof(display_buffer));
    
    // Set viewport to match framebuffer
    glViewport(0, 0, fb_width, fb_height);
    
    // Render loop with precise 240 FPS timing
    using clock = std::chrono::high_resolution_clock;
    using microseconds = std::chrono::microseconds;
    
    constexpr auto frame_time = microseconds(4167); // Exactly 1/240 seconds
    auto next_frame = clock::now();
    
    while (!glfwWindowShouldClose(window)) {
        // Update viewport if window resizes
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glUniform2f(resolution_loc, (float)fb_width, (float)fb_height);
        
        // Read all available audio samples to prevent buffer buildup
        size_t available = audio_capture->available_samples();
        if (available > 512) available = 512;  // Limit per frame
        size_t samples_read = audio_capture->read_samples(audio_read_buffer, available);
        
        if (samples_read > 0) {
            // Add samples directly to phase buffer
            for (size_t i = 0; i < samples_read; ++i) {
                phase_buffer[phase_write_pos] = audio_read_buffer[i];
                phase_write_pos = (phase_write_pos + 1) % PHASE_BUFFER_SIZE;
            }
        }
        
        // Determine read position based on phase lock mode
        size_t read_pos;
        
        if (phase_lock_enabled) {
            // Cross-correlation based phase locking
            
            // Update reference window periodically or when we don't have one
            if (!has_reference || frames_since_reference > 120) {  // Update every ~0.5 seconds at 240fps
                // Copy current window as reference
                size_t ref_start = (phase_write_pos + PHASE_BUFFER_SIZE - CORR_WINDOW) % PHASE_BUFFER_SIZE;
                for (size_t i = 0; i < CORR_WINDOW; ++i) {
                    reference_window[i] = phase_buffer[(ref_start + i) % PHASE_BUFFER_SIZE];
                }
                has_reference = true;
                frames_since_reference = 0;
            }
            frames_since_reference++;
            
            // Find best correlation offset
            float max_correlation = -1.0f;
            size_t best_offset = 0;
            
            // Search range: look back up to DISPLAY_SAMPLES + CORR_WINDOW
            const size_t SEARCH_RANGE = 1024;  // How far back to search
            size_t search_start = (phase_write_pos + PHASE_BUFFER_SIZE - DISPLAY_SAMPLES - SEARCH_RANGE) % PHASE_BUFFER_SIZE;
            
            // Compute correlation at different offsets
            for (size_t offset = 0; offset < SEARCH_RANGE; offset += 4) {  // Skip 4 for speed
                float correlation = 0.0f;
                float ref_energy = 0.0f;
                float sig_energy = 0.0f;
                
                // Compute normalized cross-correlation
                for (size_t i = 0; i < CORR_WINDOW; ++i) {
                    size_t idx = (search_start + offset + i) % PHASE_BUFFER_SIZE;
                    float sig_val = phase_buffer[idx];
                    float ref_val = reference_window[i];
                    
                    correlation += sig_val * ref_val;
                    sig_energy += sig_val * sig_val;
                    ref_energy += ref_val * ref_val;
                }
                
                // Normalize correlation
                if (sig_energy > 0.0f && ref_energy > 0.0f) {
                    correlation /= sqrtf(sig_energy * ref_energy);
                    
                    if (correlation > max_correlation) {
                        max_correlation = correlation;
                        best_offset = offset;
                    }
                }
            }
            
            // Fine-tune around best offset
            if (best_offset > 2 && best_offset < SEARCH_RANGE - 2) {
                for (int fine_offset = -2; fine_offset <= 2; ++fine_offset) {
                    size_t offset = best_offset + fine_offset;
                    float correlation = 0.0f;
                    float ref_energy = 0.0f;
                    float sig_energy = 0.0f;
                    
                    for (size_t i = 0; i < CORR_WINDOW; ++i) {
                        size_t idx = (search_start + offset + i) % PHASE_BUFFER_SIZE;
                        float sig_val = phase_buffer[idx];
                        float ref_val = reference_window[i];
                        
                        correlation += sig_val * ref_val;
                        sig_energy += sig_val * sig_val;
                        ref_energy += ref_val * ref_val;
                    }
                    
                    if (sig_energy > 0.0f && ref_energy > 0.0f) {
                        correlation /= sqrtf(sig_energy * ref_energy);
                        
                        if (correlation > max_correlation) {
                            max_correlation = correlation;
                            best_offset = offset;
                        }
                    }
                }
            }
            
            best_correlation = max_correlation;
            
            // Update target phase offset based on correlation
            if (max_correlation > correlation_threshold) {
                target_phase_offset = (search_start + best_offset) % PHASE_BUFFER_SIZE;
            } else {
                // Fallback target to recent data if correlation is poor
                target_phase_offset = (phase_write_pos + PHASE_BUFFER_SIZE - DISPLAY_SAMPLES) % PHASE_BUFFER_SIZE;
            }
            
            // Smoothly interpolate to the target phase offset
            if (phase_offset == 0) {
                // First time - jump directly to target
                phase_offset = target_phase_offset;
            } else {
                // Calculate phase difference (handling wraparound)
                int phase_diff = (int)target_phase_offset - (int)phase_offset;
                
                // Handle wraparound
                if (phase_diff > (int)(PHASE_BUFFER_SIZE / 2)) {
                    phase_diff -= PHASE_BUFFER_SIZE;
                } else if (phase_diff < -(int)(PHASE_BUFFER_SIZE / 2)) {
                    phase_diff += PHASE_BUFFER_SIZE;
                }
                
                // Apply smoothing
                float smooth_diff = phase_diff * (1.0f - phase_smoothing);
                phase_offset = (phase_offset + (int)smooth_diff + PHASE_BUFFER_SIZE) % PHASE_BUFFER_SIZE;
            }
            
            read_pos = phase_offset;
        } else {
            // No phase lock - absolute minimum latency
            read_pos = (phase_write_pos + PHASE_BUFFER_SIZE - DISPLAY_SAMPLES) % PHASE_BUFFER_SIZE;
            has_reference = false;  // Reset reference when disabled
            phase_offset = 0;  // Reset phase offset
        }
        
        // Fill display buffer
        static int debug_counter = 0;
        float max_sample = 0;
        float min_sample = 0;
        
        for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
            float sample = phase_buffer[read_pos];
            display_buffer[i].x = sample;
            display_buffer[i].y = 0.0f;
            display_buffer[i].z = 0.0f;
            display_buffer[i].w = 0.0f;
            max_sample = std::max(max_sample, sample);
            min_sample = std::min(min_sample, sample);
            read_pos = (read_pos + 1) % PHASE_BUFFER_SIZE;
        }
        
        // Debug output less frequently at high FPS
        if (++debug_counter % 1000 == 0 && (max_sample - min_sample) > 0.01f) {
            std::cout << "Phase Lock: " << (phase_lock_enabled ? "ON" : "OFF");
            if (phase_lock_enabled) {
                std::cout << " | Correlation: " << std::fixed << std::setprecision(2) << best_correlation;
                std::cout << " | Lock: " << (best_correlation > correlation_threshold ? "GOOD" : "POOR");
            }
            std::cout << " | Press 'P' to toggle" << std::endl;
        }
        
        // Upload only what we need to GPU
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        void* ptr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            memcpy(ptr, display_buffer, sizeof(display_buffer));
            glUnmapBuffer(GL_UNIFORM_BUFFER);
        }
        
        // Set uniforms
        glUniform1i(sample_loc, 0);
        glUniform1f(trigger_level_loc, best_correlation);  // Show correlation as "trigger level"
        glUniform1i(phase_lock_enabled_loc, phase_lock_enabled ? 1 : 0);
        
        // Clear to dark background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw fullscreen triangle (actually 2 triangles)
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // Precise frame timing for exactly 240 FPS
        next_frame += frame_time;
        
        // Sleep until it's time for the next frame
        auto now = clock::now();
        if (next_frame > now) {
            // Use sleep_until for more precise timing
            std::this_thread::sleep_until(next_frame);
        } else {
            // We're behind schedule, reset timing
            next_frame = now;
        }
        
        // Calculate and print FPS + stats
        fps_frame_count++;
        double current_time = glfwGetTime();
        double fps_delta = current_time - last_fps_time;
        
        if (fps_delta >= 2.0) {  // Every 2 seconds
            double fps = fps_frame_count / fps_delta;
            auto stats = audio_capture->get_stats();
            
            // Estimate total latency: audio buffer + phase buffer
            size_t total_buffered = stats.available_samples + (phase_write_pos > DISPLAY_SAMPLES ? DISPLAY_SAMPLES : phase_write_pos);
            double latency_ms = total_buffered / 44.1;  // 44.1kHz sample rate
            
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps
                      << " | Latency: ~" << std::setprecision(1) << latency_ms << "ms"
                      << " | Mode: " << (app_state.capture_input ? "INPUT" : "LOOPBACK")
                      << " | Audio buf: " << stats.available_samples
                      << " | Overruns: " << stats.overruns
                      << " | Underruns: " << stats.underruns 
                      << " | Press 'I' to switch input, 'P' for phase lock" << std::endl;
            
            fps_frame_count = 0;
            last_fps_time = current_time;
        }
    }

    audio_capture->stop();
    glfwTerminate();
    return 0;
}