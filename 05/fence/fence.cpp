/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-11-15 13:41:14 星期三
* Filename      : fence.cpp
* Description   : Fence同步机制示例
************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <WindowSurface.h>
#include <EGLUtils.h>
#include <ui/Fence.h>

using namespace android;

int main() {
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
  EGLDisplay dpy = nullptr;
  EGLint numConfigs = -1;
  EGLConfig config;
  WindowSurface windowSurface;
  EGLNativeWindowType window;
  EGLint result;
  EGLint fencefd  = -1;
  int ret = -1;

  dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(dpy, &majorVersion, &minorVersion);
  window = windowSurface.getSurface();
  eglChooseConfig(dpy, s_configAttribs, &config, 1, &numConfigs);
  surface = eglCreateWindowSurface(dpy, config, window, NULL);
  context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
  eglMakeCurrent(dpy, surface, surface, context);

  EGLSyncKHR sync = eglCreateSyncKHR(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
  fencefd = eglDupNativeFenceFDANDROID(dpy, sync);

  if(fencefd < 0)
    fencefd = 0;

  sp<Fence> fence = new Fence(fencefd);
  ret = fence->wait(-1);
  if(ret == NO_ERROR)
    printf("%s() [%d], ret = %d\n",__FUNCTION__,__LINE__,ret);
  else
    printf("%s() [%d], ret = %d\n",__FUNCTION__,__LINE__,ret);
  return 0;
}
