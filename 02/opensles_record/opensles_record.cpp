/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:25:17 星期六
* Filename      : opensl_es_record.cpp
* Description   : OpenSL ES录音示例
************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <thread>

using namespace std;

static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

static SLObjectItf recorderObject = NULL;
static SLRecordItf recorderRecord;
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

#define SL_ANDROID_SPEAKER_QUAD (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
        | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_5DOT1 (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
        | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY| SL_SPEAKER_BACK_LEFT \
        | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_7DOT1 (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT \
        | SL_SPEAKER_SIDE_RIGHT)

#define FRAMES_SIZE 1024

typedef struct Record_Data{
  FILE  *file;
  short buffer[FRAMES_SIZE];
  char  path_name[128];
  int   frame_size;
  int   sample;
  int   channel;
  int   format;
}Record_Data;

Record_Data *rec_data = (Record_Data *)malloc(sizeof(Record_Data));

void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context){
  Record_Data *data = (Record_Data *)context;
  fwrite(data->buffer, sizeof(char), rec_data->frame_size, data->file);
  fflush(data->file);
}

void opensl_record(int sampleRate, int numChannel, int format){
  slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);

  (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);

  (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);

  SLuint32 mSampleRate = 0,channelMask = 0;

  mSampleRate = sampleRate * 1000;

  if (numChannel == 1) {
    channelMask = SL_SPEAKER_FRONT_CENTER;
  }else if(numChannel == 2){
    channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  }else if(numChannel == 3){
    channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT | SL_SPEAKER_FRONT_CENTER;
  }else if(numChannel == 4){
    channelMask = SL_ANDROID_SPEAKER_QUAD;
  }else if(numChannel == 5){
    channelMask = SL_ANDROID_SPEAKER_QUAD | SL_SPEAKER_FRONT_CENTER;
  }else if(numChannel == 6){
    channelMask = SL_ANDROID_SPEAKER_5DOT1;
  }else if(numChannel == 7){
    channelMask = SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_BACK_CENTER;
  }else if(numChannel == 8){
    channelMask = SL_ANDROID_SPEAKER_7DOT1;
  }

  SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
  SLDataSource audioSrc = {&loc_dev, NULL};

  SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

  SLDataFormat_PCM format_pcm;
  format_pcm.formatType = SL_DATAFORMAT_PCM;
  format_pcm.numChannels = static_cast<SLuint32>(numChannel);
  format_pcm.samplesPerSec = mSampleRate;
  format_pcm.bitsPerSample = format;
  format_pcm.containerSize = format;
  format_pcm.channelMask = channelMask;
  format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

  SLDataSink audioSnk = {&loc_bq, &format_pcm};

  const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
  const SLboolean req[1] = {SL_BOOLEAN_TRUE};
  (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc, &audioSnk, 1, id, req);

  (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);

  (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);

  (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recorderBufferQueue);

  (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback,rec_data);

  (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
}

void read_record_data_task(){
    while(1){
      (*recorderBufferQueue)->Enqueue(recorderBufferQueue, rec_data->buffer, rec_data->frame_size);
    }
  }

int main(int argc, char* const argv[]){
  if(argc < 5){
    printf("usage: ./test /sdcard/test.pcm sample_rate channel format\n");
    return -1;
  }

  FILE *file = fopen(argv[1], "w");

  rec_data->file = file;
  rec_data->sample = atoi(argv[2]);
  rec_data->channel = atoi(argv[3]);
  rec_data->format = atoi(argv[4]);
  rec_data->frame_size = FRAMES_SIZE;
  memcpy(rec_data->path_name, argv[1], strlen(argv[1]));

  opensl_record(rec_data->sample, rec_data->channel, rec_data->format);
  printf("Write file: %s\n", rec_data->path_name);

  thread t1(read_record_data_task);
  t1.join();
  return 0;
}
