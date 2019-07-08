#include <iostream>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/loopback_recorder.h>
#include <audio_filters/filters.h>
#include <chrono>
#include <thread>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <assert.h>
#include <limits>

const uint32_t width = 2400;

std::mutex mtx;

class Initializer
{
public:
    Initializer()
    {
      std::ios_base::sync_with_stdio(false);
    }
};

static bool capturing = true;

struct vec4
{
    float x;
    float y;
    float z;
    float w;
};

const uint32_t BUFFER_LENGTH = width * 1000;

static vec4 samples[BUFFER_LENGTH];

static int current_sample = 0;

bool audio_callback(const audio::AudioBuffer &buffer)
{

  mtx.lock();
  static uint32_t step = 0;
  std::vector<float> new_samples;
  for (const audio::StereoPacket &packet : buffer) {
    //if (step++ % 2 == 0)
    //  continue;
    new_samples.push_back(packet.left);
  }

  //auto filtered = audio::filters::lowpass(new_samples);
  //std::cout << filtered.size() << " " << new_samples.size();

  for(auto& sample : new_samples)
  {
    samples[current_sample].x = (sample + 1.0F) / 2.0F;
    current_sample = (current_sample + 1) % BUFFER_LENGTH;
  }


  mtx.unlock();

  return capturing;
}

std::string load_file(const std::string &filename)
{
  std::stringstream ss;
  std::ifstream test(filename, std::ios::binary);
  ss << test.rdbuf();
  return ss.str();
}

void check_shader_compilation(uint32_t shader)
{
  int success;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
  }
}

void check_shader_link(uint32_t shader)
{
  int success;
  char infoLog[512];
  glGetShaderiv(shader, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
  }
}

/// Find Samples is used to keep the phase of the drawn waveform the same, if possible.
/// \param pattern Sample pattern to match
/// \param new_samples Samples to search for the pattern
/// \return Best matching index
int find_sample(const std::vector<float> &pattern, const std::vector<float> &new_samples)
{
  std::vector<float> conv(new_samples.size());
  for(auto& result : conv)
  {
    result = std::numeric_limits<float>::infinity();
  }
  for (int i = 0; i < new_samples.size(); i++) {
    float result = 0.0F;
    int j = 0;
    for (auto &sample : pattern) {

      if ((i + j) > new_samples.size() - 1) {
        result = std::numeric_limits<float>::infinity();
        break;
      }
      auto error = std::abs(sample - new_samples[i + j++]) + 0.00001*j;
      result += error;
      //result += sample * new_samples[i + j++];//error*error;
    }
    conv[i] = result;
  }
  return std::min_element(conv.begin(), conv.end()) - conv.begin();
}

int main()
{
  Initializer _init;

  std::cout << "Using Default Sink" << std::endl;
  std::cout << audio::get_default_sink() << std::endl;
  audio::AudioSinkInfo default_sink = audio::get_default_sink();

  audio::capture_data(&audio_callback, default_sink);

  std::string soundwave_shader_text = load_file("soundwave.glsl");
  std::string basic_vertex_text = load_file("basic_vertex.glsl");

  GLFWwindow *window;

  /* Initialize the library */
  if (!glfwInit())
    return -1;

  /* Create a windowed mode window and its OpenGL context */
  window = glfwCreateWindow(width, 800, "Audio Visualizer", NULL, NULL);
  if (!window) {
    std::cout << "error 1";
    glfwTerminate();
    return -1;
  }

  /* Make the window's context current */
  glfwMakeContextCurrent(window);
  gladLoadGL();

  unsigned int VBO;
  glGenBuffers(1, &VBO);

  unsigned int VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  float vertices[] = {
      -1.0f, -1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,

  };
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
  glEnableVertexAttribArray(0);
  uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
  auto test = basic_vertex_text.c_str();
  glShaderSource(vertexShader, 1, &test, NULL);
  glCompileShader(vertexShader);

  check_shader_compilation(vertexShader);
  unsigned int fragmentShader;
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  auto source2 = soundwave_shader_text.c_str();
  glShaderSource(fragmentShader, 1, &source2, NULL);
  glCompileShader(fragmentShader);

  check_shader_compilation(fragmentShader);

  unsigned int shaderProgram;

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  check_shader_link(shaderProgram);

  glUseProgram(shaderProgram);

  unsigned int ubo;
  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(samples), &samples, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  unsigned int block_index = glGetUniformBlockIndex(shaderProgram, "SamplesBlock");
  std::cout << "block_index" << block_index;
  GLuint binding_point_index = 2;
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBindBufferBase(GL_UNIFORM_BUFFER, binding_point_index, ubo);
  glUniformBlockBinding(shaderProgram, block_index, binding_point_index);
  GLint loc = glGetUniformLocation(shaderProgram, "current_sample");

  double previous_time = 1.0F;

  uint32_t previous_sample = current_sample;
  float a_compensation = 0.0f;
  const float samples_per_a_cycle = 48000.0f / 440.0f;

  bool running = true;
  glfwSwapInterval(0);
  /* Loop until the user closes the window */
  while (capturing) {
    mtx.lock();
    uint32_t curr_sample = current_sample;
    uint32_t samples_diff =
        curr_sample >= previous_sample ? (curr_sample - previous_sample) : ((BUFFER_LENGTH + curr_sample)
            - previous_sample);



    std::vector<float> prev;
    for (int i = 0, sample_no = previous_sample; i < 400; i++) {
      prev.push_back(samples[sample_no--].x);
      if (sample_no < 0)
        sample_no = BUFFER_LENGTH - 1;
    }

    std::vector<float> curr;
    const uint32_t lookback = 1400;
    for (int i = 0, sample_no = current_sample; i < lookback ; i++) {
      curr.push_back(samples[sample_no--].x);
      if (sample_no < 0)
        sample_no = BUFFER_LENGTH - 1;
    }

    auto sample_loc = find_sample(prev, curr);
    //std::cout << curr_sample - sample_loc - previous_sample << "\n";
    int a_sample = curr_sample - sample_loc;


    mtx.unlock();

    auto start = glfwGetTime();
    glClear(GL_COLOR_BUFFER_BIT);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    GLvoid *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);

    for (int i = 0, sample_no = a_sample; i < 2400; i++) {
      vec4 *p_gpumem = reinterpret_cast<vec4 *>(p);
      p_gpumem[i].x = samples[sample_no].x;
      sample_no = (sample_no - 1);
      if (sample_no < 0)
        sample_no = BUFFER_LENGTH - 1;
    }


    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glUniform1i(loc, a_sample);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* Swap front and back buffers */
    glfwSwapBuffers(window);

    /* Poll for and process events */
    glfwPollEvents();
    capturing = !glfwWindowShouldClose(window);

    previous_sample = a_sample;

  }

  glfwTerminate();


  return 0;
}