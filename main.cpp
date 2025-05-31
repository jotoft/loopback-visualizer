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
#include <complex>
// TODO: Implement custom FFT here
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
    float new_sample = packet.left*0.5F + packet.right*0.5F;
    new_sample *= 1.0F;
    new_samples.push_back(new_sample);

  }
  std::vector<std::complex<float>> to_transform(1024);
  for(int i = 0, c = current_sample; i < 1024; i++)
  {
    to_transform.push_back({samples[c].x, 0.0F});
    c+=1;
    c%= BUFFER_LENGTH;
  }

  //auto filtered = audio::filters::lowpass(new_samples);
  //std::cout << filtered.size() << " " << new_samples.size();

  for(int i = 1; i < new_samples.size(); i++)
  {
      auto& sample_now = new_samples[i-1];
      auto& sample_next = new_samples[i];

      auto sample_interp = (sample_now + sample_next) / 2.0F;

      float k = sample_next-sample_now;
      auto sample1 = k*0.25 + sample_now;
      auto sample2 = k*0.5 + sample_now;
      auto sample3 = k*0.75 + sample_now;

      samples[current_sample].x = sample_now;
      current_sample = (current_sample + 1) % BUFFER_LENGTH;

      samples[current_sample].x = sample1;
      current_sample = (current_sample + 1) % BUFFER_LENGTH;


      samples[current_sample].x = sample2;
      current_sample = (current_sample + 1) % BUFFER_LENGTH;

      samples[current_sample].x = sample3;
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
      auto error = (sample - new_samples[i + j++]);

      result += error*error;
    }
    conv[i] = result;
  }
  return std::min_element(conv.begin(), conv.end()) - conv.begin();
}

int main()
{
  Initializer _init;
  const bool capture = false;
  std::cout << "Using Default Sink" << std::endl;
  std::cout << audio::get_default_sink(capture) << std::endl;
  audio::AudioSinkInfo default_sink = audio::get_default_sink(capture);

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
  glfwSwapInterval(1);
  /* Loop until the user closes the window */
    glClear(GL_COLOR_BUFFER_BIT);
  while (capturing) {
    mtx.lock();
    uint32_t curr_sample = current_sample;
    uint32_t samples_diff =
        curr_sample >= previous_sample ? (curr_sample - previous_sample) : ((BUFFER_LENGTH + curr_sample)
            - previous_sample);


    const int stride = 4;

    std::vector<float> prev;
    //std::cout << previous_sample % stride;
    for (int i = 0, sample_no = previous_sample - (previous_sample % stride); i < 600; i++) {
      prev.push_back(samples[sample_no].x);
      sample_no -= stride;
      if (sample_no < 0)
        sample_no = BUFFER_LENGTH - 1;
    }

    std::vector<float> curr;
    const uint32_t lookback = 1800;
    for (int i = 0, sample_no = current_sample - (current_sample % stride); i < lookback ; i++) {
      curr.push_back(samples[sample_no].x);
      sample_no -= stride;
      if (sample_no < 0)
        sample_no = BUFFER_LENGTH - 1;
    }

    auto sample_loc = find_sample(prev, curr);
    int a_sample = curr_sample - sample_loc*stride;


    mtx.unlock();

    auto start = glfwGetTime();
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    GLvoid *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);

    for (int i = 2399, sample_no = a_sample; i >= 0; i--) {
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