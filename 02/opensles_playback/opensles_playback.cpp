/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-05-21 23:50:21 星期日
* Filename      : opensles_playback.cpp
* Descriptionrepeat   : OpenSL ES播放示例
************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <utils/Log.h>
#include <media/MediaMetrics.h>
#include <utils/CallStack.h>

#define MAX_NUMBER_INTERFACES 3

static SLObjectItf engineObject;
static SLEngineItf EngineItf;
static SLSeekItf fdPlayerSeek;

SLObjectItf  player, outputMix;
SLDataSource            audioSource;
SLDataLocator_AndroidFD locatorFd;
SLDataFormat_MIME       mime;

SLDataSink               audioSink;
SLDataLocator_OutputMix  locator_outputmix;

SLPlayItf              playItf;
SLPrefetchStatusItf    prefetchItf;
SLboolean required[MAX_NUMBER_INTERFACES];
SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

void playback_test(const char* path){
  slCreateEngine(&engineObject, 0, NULL , 0, NULL, NULL);

  (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);

  (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, (void*)&EngineItf);

  for (int i=0 ; i < MAX_NUMBER_INTERFACES ; i++) {
    required[i] = SL_BOOLEAN_FALSE;
    iidArray[i] = SL_IID_NULL;
  }

  (*EngineItf)->CreateOutputMix(EngineItf, &outputMix, 1, iidArray, required);

  (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);

  locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  locator_outputmix.outputMix   = outputMix;
  audioSink.pLocator            = (void*)&locator_outputmix;
  audioSink.pFormat             = NULL;

  required[0] = SL_BOOLEAN_TRUE;
  required[1] = SL_BOOLEAN_TRUE;
  required[2] = SL_BOOLEAN_TRUE;

  iidArray[0] = SL_IID_PREFETCHSTATUS;
  iidArray[1] = SL_IID_EQUALIZER;
  iidArray[2] = SL_IID_ANDROIDCONFIGURATION;

  int fd = open(path, O_RDONLY);
  locatorFd.locatorType = SL_DATALOCATOR_ANDROIDFD;
  locatorFd.fd = (SLint32) fd;
  locatorFd.length = SL_DATALOCATOR_ANDROIDFD_USE_FILE_SIZE; //length: all mp3 size.
  locatorFd.offset = 0; //offset = 0;起始地址.

  mime.formatType = SL_DATAFORMAT_MIME;
  mime.mimeType      = (SLchar*)NULL;
  mime.containerType = SL_CONTAINERTYPE_UNSPECIFIED;
  audioSource.pFormat  = (void*)&mime;
  audioSource.pLocator = (void*)&locatorFd;

  (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &audioSource, &audioSink, 3, iidArray, required);


  {
    SLAndroidConfigurationItf playerConfig;
    (*player)->GetInterface(player, SL_IID_ANDROIDCONFIGURATION, &playerConfig);
    SLresult result;
        
    SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
    result = (*playerConfig)->SetConfiguration(playerConfig,SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));    
    SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_LATENCY_EFFECTS;
    result = (*playerConfig)->SetConfiguration(playerConfig, SL_ANDROID_KEY_PERFORMANCE_MODE, &performanceMode, sizeof(SLuint32));   
    
    SLuint32 performanceModeSize = sizeof(performanceMode);
    result = (*playerConfig)->GetConfiguration(playerConfig, SL_ANDROID_KEY_PERFORMANCE_MODE, &performanceModeSize, &performanceMode);
  }
  
  (*player)->Realize(player, SL_BOOLEAN_FALSE);

  (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf);

  (*player)->GetInterface(player, SL_IID_PREFETCHSTATUS, (void*)&prefetchItf);

  (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PAUSED );

  SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
  while(prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) {
    usleep(100 * 1000);
    (*prefetchItf)->GetPrefetchStatus(prefetchItf, &prefetchStatus);
  }

  SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
  
  (*playItf)->GetDuration(playItf, &durationInMsec);
  printf("durationInMsec = %u\n",durationInMsec);
  if(durationInMsec == SL_TIME_UNKNOWN)
    durationInMsec = 500 * 1000;//500s
    
  (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
  usleep(durationInMsec * 1000);
  
  (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);

  (*player)->Destroy(player);

  (*outputMix)->Destroy(outputMix);

  (*engineObject)->Destroy(engineObject);
}

int main(int argc, char* const argv[]){
  fprintf(stdout, "usage: \"%s /sdcard/my.mp3\" \n", argv[0]);
  char *file_name = argv[1];

  playback_test(file_name);
  return 0;
}
