/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:28:40 星期六
* Filename      : oboe_playback.cpp
* Description   : OBOE播放示例
************************************************************/

#include <oboe/AudioStreamBuilder.h>
#include <oboe/AudioStream.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>

using namespace oboe;

FILE *file = nullptr;
int index = 0;
long fileLength = 0;
int kChannelCount = 2;
int kSampleRate = 44100;
AudioFormat kFormat = oboe::AudioFormat::I16;
std::mutex         mLock;
std::shared_ptr<oboe::AudioStream> stream;

class AudioPlay : public oboe::AudioStreamCallback{
public:
  oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *audioStream,
	void *audioData,
	int32_t numFrames) override;

  void startPlay(char *path);
};

oboe::DataCallbackResult AudioPlay::onAudioReady(
      oboe::AudioStream *audioStream,
      void *audioData,
      int32_t numFrames) {

  fread(audioData, numFrames * sizeof(int16_t) * kChannelCount , 1, file);
  index += numFrames * sizeof(int16_t) * kChannelCount;

  if (index < fileLength)
    return oboe::DataCallbackResult::Continue;
  else
    return oboe::DataCallbackResult::Stop;
}

void AudioPlay::startPlay(char *path){
  file = fopen(path, "rb+");
  if (file != nullptr) {
    int fd1 = fileno(file);
    fileLength = lseek(fd1, 0, SEEK_END);
    fseek(file, 0, SEEK_SET);
  }

  std::lock_guard<std::mutex> lock(mLock);
  oboe::AudioStreamBuilder *builder = new oboe::AudioStreamBuilder();
  builder->setSharingMode(oboe::SharingMode::Exclusive);
  builder->setPerformanceMode(oboe::PerformanceMode::LowLatency);
  builder->setChannelCount(kChannelCount);
  builder->setSampleRate(kSampleRate);
  builder->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
  builder->setFormat(kFormat);
  builder->setDataCallback(this);
  builder->openStream(stream);

  stream->requestStart();
}

int main(int argc, char *argv[]){
  if(argc < 2){
    fprintf(stdout, "usage: \"%s /sdcard/play.pcm\" \n", argv[0]);
    return -1;
  }

  char *file_name = argv[1];
  AudioPlay *play = new AudioPlay;
  play->startPlay(file_name);
  usleep(500 * 1000 * 1000);
  stream->requestStop();

  fclose(file);
}
