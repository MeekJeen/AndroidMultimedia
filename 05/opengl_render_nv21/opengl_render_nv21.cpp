/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-11-19 23:38:19 星期日
* Filename      : opengl_render_nv21.cpp
* Description   : Opengl渲染NV21视频数据示例
************************************************************/

#include <stdio.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DynamicDisplayInfo.h>
using namespace android;

static int ALIGN(int x, int y) {
  return (x + y - 1) & ~(y - 1);
}

void render(const void *data, size_t size, const sp<ANativeWindow> &nativeWindow,int width,int height) {
  void *dst;
  ANativeWindowBuffer *buf;
  int mCropWidth = width;
  int mCropHeight = height;
  int halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
  int bufWidth = (mCropWidth + 1) & ~1;
  int bufHeight = (mCropHeight + 1) & ~1;

  native_window_set_usage(nativeWindow.get(),GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
  native_window_api_connect(nativeWindow.get(), NATIVE_WINDOW_API_MEDIA);
  native_window_set_scaling_mode(nativeWindow.get(),NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
#if 1
  //v1.0
  (nativeWindow.get())->perform(nativeWindow.get(), NATIVE_WINDOW_SET_BUFFERS_GEOMETRY,bufWidth, bufHeight, halFormat);
#else
  //v2.0
  native_window_set_buffers_dimensions(nativeWindow.get(), bufWidth, bufHeight);
  native_window_set_buffers_format(nativeWindow.get(), halFormat);
#endif

  native_window_dequeue_buffer_and_wait(nativeWindow.get(),&buf);
  GraphicBufferMapper &mapper = GraphicBufferMapper::get();
  Rect bounds(mCropWidth, mCropHeight);

  mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);
  size_t dst_y_size = buf->stride * buf->height;
  size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
  size_t dst_c_size = dst_c_stride * buf->height / 2;
  memcpy(dst, data, dst_y_size + dst_c_size*2);
  mapper.unlock(buf->handle);

  nativeWindow->queueBuffer(nativeWindow.get(), buf,-1);
  buf = NULL;
}

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("usage: ./opengl_render_nv21 nv21.yuv\n");
    return -1;
  }

  sp<SurfaceComposerClient> client = new SurfaceComposerClient();
  sp<IBinder> mainDpy = SurfaceComposerClient::getInternalDisplayToken();
  ui::DynamicDisplayInfo mainDpyInfo;
  SurfaceComposerClient::getDynamicDisplayInfo(mainDpy, &mainDpyInfo);

  int lcd_width = mainDpyInfo.supportedDisplayModes[0].resolution.getWidth();
  int lcd_height = mainDpyInfo.supportedDisplayModes[0].resolution.getHeight();

  sp<SurfaceControl> surfaceControl = client->createSurface(String8("TSurface"),lcd_width, lcd_height, PIXEL_FORMAT_RGBA_8888, 0);
  SurfaceComposerClient::Transaction{}.setLayer(surfaceControl, 100000)
    .show(surfaceControl)
    .setPosition(surfaceControl, 0, 0)
    .setSize(surfaceControl, lcd_width, lcd_height)
    .apply();

  sp<Surface> surface = surfaceControl->getSurface();
  FILE *fp = fopen(argv[1],"rb");
  int width = 640;
  int height = 480;

  int size = width * height * 3/2;
  std::vector<unsigned char> data(size);

  while(feof(fp) == 0){
    int num  = fread(data.data(), 1, size, fp);
    usleep(40 * 1000);
    render(data.data(),size,surface,width,height);
  }
  return 0;
}
