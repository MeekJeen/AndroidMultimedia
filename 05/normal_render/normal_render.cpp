/***********************************************************
 * Author        : 公众号: Android系统攻城狮
 * Create time   : 2023-07-24 01:26:21 星期一
 * Filename      : normal_render.cpp
 * Description   : Opengles渲染三角形示例
 ************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <WindowSurface.h>
using namespace android;

GLuint gProgram;
GLuint gvPositionHandle;
const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };

static const char gVertexShader[] = "attribute vec4 vPosition;\n"
  "void main() {\n"
  "  gl_Position = vPosition;\n"
  "}\n";

static const char gFragmentShader[] = "precision mediump float;\n"
  "void main() {\n"
  "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
  "}\n";

GLuint loadShader(GLenum shaderType, const char* pSource) {
  GLuint shader = glCreateShader(shaderType);
  if (shader) {
    glShaderSource(shader, 1, &pSource, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      GLint infoLen = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
      if (infoLen) {
	char* buf = (char*) malloc(infoLen);
	if (buf) {
	  glGetShaderInfoLog(shader, infoLen, NULL, buf);
	  free(buf);
	}
	glDeleteShader(shader);
	shader = 0;
      }
    }
  }
  return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
  GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
  if (!vertexShader) {
    return 0;
  }

  GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
  if (!pixelShader) {
    return 0;
  }

  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertexShader);
    glAttachShader(program, pixelShader);
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
      GLint bufLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
      if (bufLength) {
	char* buf = (char*) malloc(bufLength);
	if (buf) {
	  glGetProgramInfoLog(program, bufLength, NULL, buf);
	  free(buf);
	}
      }
      glDeleteProgram(program);
      program = 0;
    }
  }
  return program;
}

void initShader(int w, int h) {
  gProgram = createProgram(gVertexShader, gFragmentShader);
  gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
}

void renderFrame() {
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(gProgram);
  glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
  glEnableVertexAttribArray(gvPositionHandle);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

int main(int argc, char** argv) {
  EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  EGLint s_configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE };
  EGLint majorVersion;
  EGLint minorVersion;
  EGLContext context;
  EGLSurface surface;
  EGLint w, h;
  EGLDisplay dpy;
  EGLint numConfigs = -1;
  EGLConfig config;
  WindowSurface windowSurface;
  EGLNativeWindowType window;

  dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(dpy, &majorVersion, &minorVersion);
  window = windowSurface.getSurface();
  eglChooseConfig(dpy, s_configAttribs, &config, 1, &numConfigs);
  surface = eglCreateWindowSurface(dpy, config, window, NULL);
  context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
  eglMakeCurrent(dpy, surface, surface, context);
  initShader(w, h);
  renderFrame();
  eglSwapBuffers(dpy, surface);
  getchar();
  return 0;
}
