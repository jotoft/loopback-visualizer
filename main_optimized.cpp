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

// Much smaller display buffer - we only need what's visible
const uint32_t DISPLAY_SAMPLES = width;
static float display_buffer[DISPLAY_SAMPLES];

// Pre-allocated buffers to avoid allocations in render loop
static float audio_read_buffer[4096];

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


int main()
{
    Initializer _init;
    const bool capture = false;
    
    // Get default sink using Option type
    auto default_sink_opt = audio::get_default_sink(capture);
    if (default_sink_opt.is_none()) {
        std::cerr << "No default audio sink found" << std::endl;
        return -1;
    }
    
    auto default_sink = default_sink_opt.unwrap();
    std::cout << "Using Default Sink" << std::endl;
    std::cout << default_sink << std::endl;

    // Create lock-free audio capture
    auto audio_capture = audio::create_audio_capture(default_sink);
    auto result = audio_capture->start();
    if (result.is_err()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        return -1;
    }

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
    window = glfwCreateWindow(width, 800, "Audio Visualizer (Optimized)", NULL, NULL);
    
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

    // Timing
    glfwSwapInterval(1); // VSync
    
    // Pre-allocate circular buffer for phase tracking
    const size_t PHASE_BUFFER_SIZE = 16384;
    float phase_buffer[PHASE_BUFFER_SIZE] = {0}; // Initialize to zero
    size_t phase_write_pos = 0;
    
    // Initialize display buffer to zero to avoid garbage
    std::memset(display_buffer, 0, sizeof(display_buffer));
    
    // Set viewport to match framebuffer
    glViewport(0, 0, fb_width, fb_height);
    
    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Update viewport if window resizes
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glUniform2f(resolution_loc, (float)fb_width, (float)fb_height);
        // Read audio samples without blocking
        size_t samples_read = audio_capture->read_samples(audio_read_buffer, 512);
        
        if (samples_read > 0) {
            // Add samples directly to phase buffer (no CPU interpolation needed)
            for (size_t i = 0; i < samples_read; ++i) {
                phase_buffer[phase_write_pos] = audio_read_buffer[i];
                phase_write_pos = (phase_write_pos + 1) % PHASE_BUFFER_SIZE;
            }
        }
        
        // Fill display buffer from phase buffer
        size_t read_pos = (phase_write_pos + PHASE_BUFFER_SIZE - DISPLAY_SAMPLES) % PHASE_BUFFER_SIZE;
        
        // Check if we have any non-zero samples
        static int debug_counter = 0;
        float max_sample = 0;
        
        for (size_t i = 0; i < DISPLAY_SAMPLES; ++i) {
            display_buffer[DISPLAY_SAMPLES - 1 - i] = phase_buffer[read_pos];
            max_sample = std::max(max_sample, std::abs(phase_buffer[read_pos]));
            read_pos = (read_pos + 1) % PHASE_BUFFER_SIZE;
        }
        
        // Debug output every 60 frames
        if (++debug_counter % 60 == 0) {
            std::cout << "Max sample: " << max_sample << " | Write pos: " << phase_write_pos << std::endl;
        }
        
        // Upload only what we need to GPU
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        void* ptr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(display_buffer),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            memcpy(ptr, display_buffer, sizeof(display_buffer));
            glUnmapBuffer(GL_UNIFORM_BUFFER);
        }
        
        // Set current sample position
        glUniform1i(sample_loc, 0);
        
        // Clear to dark background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // Draw fullscreen triangle (actually 2 triangles)
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // Print stats occasionally
        static int frame_count = 0;
        if (++frame_count % 120 == 0) {  // Less frequent output
            auto stats = audio_capture->get_stats();
            std::cout << "Stats - Available: " << stats.available_samples
                      << ", Overruns: " << stats.overruns
                      << ", Underruns: " << stats.underruns << std::endl;
        }
    }

    audio_capture->stop();
    glfwTerminate();
    return 0;
}