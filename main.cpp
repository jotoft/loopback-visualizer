#include <iostream>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/loopback_recorder.h>
#include <chrono>
#include <thread>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
class Initializer
{
public:
    Initializer()
    {
      std::ios_base::sync_with_stdio(false);
    }
};

static bool capturing = true;

bool audio_callback(const audio::AudioBuffer &buffer)
{
    std::cout << buffer.rbegin()->left << std::endl;
    return capturing;
}

std::string load_file(const std::string& filename)
{
    std::stringstream ss;
    std::ifstream test(filename, std::ios::binary);
    ss << test.rdbuf();
    return ss.str();
}


void glfw_render()
{
}

void check_shader_compilation(uint32_t shader)
{
  int  success;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if(!success)
  {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
  }
}


void check_shader_link(uint32_t shader)
{
  int  success;
  char infoLog[512];
  glGetShaderiv(shader, GL_LINK_STATUS, &success);
  if(!success)
  {
    glGetProgramInfoLog(shader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
  }
}

int main()
{
  Initializer _init;
  //for(auto& sink: audio::list_sinks())
  //{
  //  std::cout << sink << std::endl;
  //}


  std::cout << "Using Default Sink" << std::endl;
  std::cout << audio::get_default_sink() << std::endl;
  audio::AudioSinkInfo default_sink = audio::get_default_sink();

  audio::capture_data(&audio_callback, default_sink);

  std::string soundwave_shader_text = load_file("soundwave.glsl");
  std::string basic_vertex_text = load_file("basic_vertex.glsl");

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1200, 800, "Hello World", NULL, NULL);
    if (!window)
    {
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
      -0.5f, -0.5f, 0.0f,
      0.5f, -0.5f, 0.0f,
      0.0f,  0.5f, 0.0f
  };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
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






  bool running = true;
    /* Loop until the user closes the window */
    while (capturing)
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, 3);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
        capturing = !glfwWindowShouldClose(window);
    }

    glfwTerminate();


  return 0;
}