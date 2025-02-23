/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-07-26 12:23:24 星期三
* Filename      : offscreen_render.cpp
* Description   : Opengles离屏渲染示例
************************************************************/

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fstream>

const GLchar* vertexShaderSource = "#version 300 es\n"
  "layout (location = 0) in vec3 aPos;\n"
  "void main()\n"
  "{\n"
  "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
  "}\0";

const GLchar* fragmentShaderSource = "#version 300 es\n"
  "out vec4 FragColor;\n"
  "void main()\n"
  "{\n"
  "   FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
  "}\n\0";

GLfloat vertices[] = {
  -0.5f, -0.5f, 0.0f,
  0.5f, -0.5f, 0.0f,
  0.0f,  0.5f, 0.0f
};

GLuint createShader(GLenum type, const GLchar* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  return shader;
}

GLuint createProgram(GLuint vertexShader, GLuint fragmentShader) {
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  return program;
}

void savePixels(const char* filename, GLubyte* pixels, int width, int height) {
  std::ofstream file(filename, std::ios::binary);
  file << "P6\n" << width << " " << height << "\n255\n";
  file.write(reinterpret_cast<char*>(pixels), width * height * 3);
}

int main() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, NULL, NULL);
  const EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_DEPTH_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_NONE
  };

  EGLConfig config;
  EGLint numConfigs;
  eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);

  const EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };
  EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

  const EGLint pbufferAttribs[] = {
    EGL_WIDTH, 800,
    EGL_HEIGHT, 600,
    EGL_NONE,
  };
  EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
  eglMakeCurrent(display, surface, surface, context);
  GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
  GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
  GLuint program = createProgram(vertexShader, fragmentShader);
  GLuint vbo;

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

  glUseProgram(program);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  GLubyte* pixels = new GLubyte[800 * 600 * 3];
  glReadPixels(0, 0, 800, 600, GL_RGB, GL_UNSIGNED_BYTE, pixels);

  savePixels("/data/debug/output.ppm", pixels, 800, 600);

  delete[] pixels;
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(program);
  glDeleteShader(fragmentShader);
  glDeleteShader(vertexShader);

  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(display, surface);
  eglDestroyContext(display, context);
  eglTerminate(display);
  return 0;
}
