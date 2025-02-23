/***********************************************************
 * Author        : 公众号: Android系统攻城狮
 * Create time   : 2023-06-11 23:58:05 星期日
 * Filename      : camera_recorder.cpp
 * Description   : 采集Camera NV21数据并预览示例
 ************************************************************/

#include <binder/ProcessState.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <camera/CameraParameters.h>
#include <camera/CameraParameters2.h>
#include <camera/CameraMetadata.h>
#include <camera/Camera.h>
#include <android/hardware/ICameraService.h>
#include <utility>
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayState.h>
#include <thread>
#include <mutex>

using namespace android;
using namespace android::hardware;
static FILE *fp;

class MyListenser : public CameraListener{
public:
  MyListenser(){
    fp= fopen("/data/debug/yuv_data.yuv", "w");
  }

  void postData(int32_t msgType, const sp<IMemory>& dataPtr, camera_frame_metadata_t *metadata) override {
    if(msgType == CAMERA_MSG_PREVIEW_FRAME){
      fwrite(dataPtr->unsecurePointer(), 1, dataPtr->size(), fp);
      fflush(fp);
    }
    printf("msgType = %d\n",msgType);
  }
  void notify(int32_t msgType, int32_t ext1, int32_t ext2){}
  void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr){}
  void postRecordingFrameHandleTimestamp(nsecs_t timestamp, native_handle_t* handle){}
  void postRecordingFrameHandleTimestampBatch(const std::vector<nsecs_t>& timestamps,const std::vector<native_handle_t*>& handles){}
};

class CameraClientTest : public Camera {
public:
  CameraClientTest() : Camera(0){}
  void notifyCallback(__unused int32_t msgType, int32_t, int32_t) override{};
  void dataCallback(int32_t msgType, const sp<IMemory>&,camera_frame_metadata_t *) override{};
  void dataCallbackTimestamp(nsecs_t timestamp, int32_t msgType,const sp<IMemory>& data) override {};
  void recordingFrameHandleCallbackTimestamp(nsecs_t,native_handle_t*) override {};
  void recordingFrameHandleCallbackTimestampBatch(const std::vector<nsecs_t>&,const std::vector<native_handle_t*>&) override {};
};

void get_camera_yuv_task(sp<Camera> cameraDevice){
  for(;;){
    cameraDevice->startPreview();
    sleep(2);
    cameraDevice->takePicture(CAMERA_MSG_COMPRESSED_IMAGE);
    sleep(2);
    cameraDevice->stopPreview();
  }
}

int main(int argc, char *argv[]){
  int32_t cameraId = 1;//0: 表示后置摄像头; 1:表示前置摄像头
  if(argc < 2){
    printf("usage:./camera_preview 0/1\n");
    return -1;
  }
  cameraId =  atoi(argv[1]);

  sp<Surface> previewSurface;
  sp<SurfaceControl> surfaceControl;
  sp<SurfaceComposerClient> mComposerClient;
  mComposerClient = new SurfaceComposerClient;
  sp<CameraClientTest> cam = new  CameraClientTest;
  sp<Camera> cameraDevice = Camera::connect(cameraId, String16("native_camera"), Camera::USE_CALLING_UID,Camera::USE_CALLING_PID, android_get_application_target_sdk_version());
  sp<MyListenser> listener = new MyListenser;

  cameraDevice->setListener(listener);
  cameraDevice->setPreviewCallbackFlags(CAMERA_FRAME_CALLBACK_FLAG_CAMERA);
  cameraDevice->sendCommand(CAMERA_CMD_SET_DISPLAY_ORIENTATION, 90, 0);

  CameraParameters2 params2(cameraDevice->getParameters());
  params2.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);
  params2.set(CameraParameters::KEY_ZOOM, 6);
  params2.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_AUTO);
  cameraDevice->setParameters(params2.flatten());

  CameraInfo cameraInfo;
  Camera::getCameraInfo(cameraId, &cameraInfo);

  int previewWidth, previewHeight;
  params2.getPreviewSize(&previewWidth, &previewHeight);
  if(cameraInfo.orientation == 90)
    std::swap(previewWidth, previewHeight);
  printf("orientation = %d\n",cameraInfo.orientation);
  printf("pre_width = %d, pre_height = %d, preview_format = %s\n",previewWidth,previewHeight,params2.getPreviewFormat());

  surfaceControl = mComposerClient->createSurface(String8("Test Surface"),previewWidth, previewHeight,
						  CameraParameters::previewFormatToEnum(params2.getPreviewFormat()),
						  GRALLOC_USAGE_HW_COMPOSER);
  previewSurface = surfaceControl->getSurface();
  cameraDevice->setPreviewTarget(previewSurface->getIGraphicBufferProducer());

#if 1
  cameraDevice->startPreview();
  IPCThreadState::self()->joinThreadPool();
#else  
  std::mutex mutex;
  std::thread t1 = std::thread([&]() {
    std::lock_guard<decltype(mutex)> lock(mutex);
    for (;;) {
      cameraDevice->startPreview();
      }
  });
  t1.join();
#endif
  
  cameraDevice->stopPreview();
  cameraDevice->disconnect();
  mComposerClient->dispose();
}
