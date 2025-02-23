/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 16:53:03 星期六
* Filename      : aaudio_playback.cpp
* Description   : AAudio播放PCM示例.
************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <aaudio/AAudio.h>

int mfd;
int32_t mSampleRate{44100};
int16_t mChannel{2};
aaudio_format_t mFormat{AAUDIO_FORMAT_PCM_I16};
AAudioStream *mAudioStream{nullptr};

aaudio_data_callback_result_t dataCallback(AAudioStream *stream,void *userData,void *audioData, int32_t numFrames) {
  int size = read(mfd, audioData, numFrames * mChannel * (mFormat == AAUDIO_FORMAT_PCM_I16 ? 2 : 1));
  if (size <= 0) {
    printf("AAudioEngine::dataCallback, file reach eof!!\n");
    return AAUDIO_CALLBACK_RESULT_STOP;
  }
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void aaudio_play(char *filePath) {  
  AAudioStreamBuilder *builder = nullptr;

  mfd = open(filePath, O_RDONLY);
  AAudio_createStreamBuilder(&builder);
  
  AAudioStreamBuilder_setDeviceId(builder, AAUDIO_UNSPECIFIED);
  AAudioStreamBuilder_setFormat(builder, mFormat);
  AAudioStreamBuilder_setChannelCount(builder, mChannel);
  AAudioStreamBuilder_setSampleRate(builder, mSampleRate);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);//播放
  AAudioStreamBuilder_setDataCallback(builder, dataCallback, NULL);

  AAudioStreamBuilder_openStream(builder, &mAudioStream);
  AAudioStream_requestStart(mAudioStream);
  usleep(500 *1000 * 1000);
  
  AAudioStream_requestStop(mAudioStream);
  AAudioStream_close(mAudioStream);
  AAudioStreamBuilder_delete(builder);
  close(mfd);
}

int main(int argc, char *argv[]){
  if(argc < 2){
    fprintf(stdout, "usage: \"%s /sdcard/play.pcm\" \n", argv[0]);
    return -1;
  }

  char *file_name = argv[1];
  aaudio_play(file_name);
  return 0;
}
