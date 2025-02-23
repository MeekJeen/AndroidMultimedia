/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:29:00 星期六
* Filename      : oboe_recorder.cpp
* Description   : OBOE录音示例
************************************************************/

#include <oboe/AudioStreamBuilder.h>
#include <oboe/AudioStream.h>
#include <unistd.h>
#include <sys/types.h>
#include<thread>

using namespace oboe;

int kChannelCount = 2;
int kSampleRate = 16000;
AudioFormat kFormat = oboe::AudioFormat::I16;

static FILE *outFile;
std::shared_ptr<oboe::AudioStream> stream;

class AudioRecord : public oboe::AudioStreamCallback{
public:
  void startRecord();
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
};

void startOfThread(AudioRecord *ar) {
  ar->startRecord();
}

oboe::DataCallbackResult AudioRecord::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
  printf("numFrames = %d\n",numFrames);
  if(numFrames > 0){
    fwrite(audioData, sizeof(int16_t), numFrames * kChannelCount, outFile);
    return oboe::DataCallbackResult::Continue;
  }else{
    return oboe::DataCallbackResult::Stop;
  }
}

void AudioRecord::startRecord() {
  oboe::AudioStreamBuilder *builder = new oboe::AudioStreamBuilder();

  builder->setDirection(oboe::Direction::Input);
  builder->setAudioApi(oboe::AudioApi::AAudio);
  builder->setPerformanceMode(oboe::PerformanceMode::LowLatency);
  builder->setSharingMode(oboe::SharingMode::Exclusive);
  builder->setFormat(kFormat);
  builder->setSampleRate(kSampleRate);
  builder->setChannelCount(kChannelCount);
  builder->setDataCallback(this);
  builder->openStream(stream);
  
  stream->requestStart();
}

int main(int argc, char *argv[]){
  if(argc < 2){
    fprintf(stdout, "usage: \"%s /sdcard/record.pcm\" \n", argv[0]);
    return -1;
  }

  outFile = fopen(argv[1], "w");
  AudioRecord *rec = new AudioRecord;

  std::thread th(startOfThread, rec);
  th.detach();

  usleep(500 * 1000 * 1000);
  stream->requestStop();
  fclose(outFile);
}
