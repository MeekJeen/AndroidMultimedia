/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-08-23 22:56:36 星期三
* Filename      : mp4_extract_h264.cpp
* Description   : NuMediaExtractor将mp4解封装为h264示例
************************************************************/

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <utils/Log.h>
#include <binder/ProcessState.h>
#include <media/IMediaHTTPService.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaMuxer.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/NuMediaExtractor.h>
#include <media/stagefright/MetaDataBase.h>

using namespace android;

void print_str2hex(unsigned char *name, unsigned char* str, size_t size){
  printf("%s: ",name);
  for (size_t i = 0; i < size; i++) {
    printf("%02x ", static_cast<unsigned char>(str[i]));
  }
  printf("\n");
}

static int muxing(const char *inputFileName, bool useAudio, bool useVideo,
		  const char *outputFileName, int rotationDegrees,
		  MediaMuxer::OutputFormat container) {
  sp<AMessage> format;
  KeyedVector<size_t, ssize_t> trackIndexMap;
  FILE *fp_out = fopen("/data/debug/output.h264","w+");
  
  sp<NuMediaExtractor> extractor = new NuMediaExtractor(NuMediaExtractor::EntryPoint::OTHER);
  extractor->setDataSource(NULL, inputFileName);
  printf("input_file = %s, output_file = %s\n", inputFileName, outputFileName);

  int fd = open(outputFileName, O_CREAT | O_LARGEFILE | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
  sp<MediaMuxer> muxer = new MediaMuxer(fd, container);
  close(fd);

  
  size_t trackCount = extractor->countTracks();
  size_t bufferSize = 1 * 1024 * 1024;
  ssize_t newTrackIndex = -1;
  
  for (size_t i = 0; i < trackCount; ++i){
    extractor->getTrackFormat(i, &format);
    printf("extractor getTrackFormat: trackindex[%zu] = %s\n",i,format->debugString().c_str());

    AString mime;
    format->findString("mime", &mime);
    printf("mime = %s\n",mime.c_str());

    bool isAudio = !strncasecmp(mime.c_str(), "audio/", 6);
    bool isVideo = !strncasecmp(mime.c_str(), "video/", 6);

    ssize_t newTrackIndex = -1;
    if (useAudio && isAudio) {
      extractor->selectTrack(i);
      newTrackIndex = muxer->addTrack(format);
      if(newTrackIndex >= 0)
	trackIndexMap.add(i, newTrackIndex);
    } else if (useVideo &&  isVideo) {
      extractor->selectTrack(i);
      newTrackIndex = muxer->addTrack(format);
      if(newTrackIndex >= 0)
	trackIndexMap.add(i, newTrackIndex);
    } else {
      continue;
    }
  }

  int64_t muxerStartTimeUs = ALooper::GetNowUs();
  bool sawInputEOS = false;

  size_t trackIndex = -1;
  sp<ABuffer> newBuffer = new ABuffer(bufferSize);

  muxer->setOrientationHint(rotationDegrees);
  muxer->start();

  while (sawInputEOS == false) {
    status_t err = extractor->getSampleTrackIndex(&trackIndex);

    if (err != OK) {
      printf("saw input eos, err %d\n", err);
      sawInputEOS = true;
      break;
    } else if (trackIndexMap.indexOfKey(trackIndex) < 0) {
      printf("skipping input from unsupported track %zu\n", trackIndex);
      extractor->advance();
      continue;
    } else {
      extractor->readSampleData(newBuffer);

      std::vector<unsigned char> kAdtsHeader;
      if(useAudio){
	uint32_t kAdtsHeaderLength = 7;
	uint32_t aac_sheader_data_length = newBuffer->size()  + kAdtsHeaderLength;
        kAdtsHeader.resize(aac_sheader_data_length);

	int profile = 2;
	int freqIdx = 3;
	int chanCfg = 1;	
	kAdtsHeader.data()[0] = 0xFF;
	kAdtsHeader.data()[1] = 0xF1;	
	kAdtsHeader.data()[2] = (((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2)) + 1;	
	kAdtsHeader.data()[3] = (((chanCfg & 3) << 7) + (aac_sheader_data_length >> 11));	
	kAdtsHeader.data()[4] = ((aac_sheader_data_length & 0x7FF) >> 3);	
	kAdtsHeader.data()[5] = (((aac_sheader_data_length & 7) << 5) + 0x1F);
	kAdtsHeader.data()[6] = 0xFC;

	fwrite(kAdtsHeader.data(), 1, kAdtsHeaderLength, fp_out);
	printf("data[0] = 0xFF, data[1] = 0xF1, data[2] = %#x, data[3] = %#x, data[4] = %#x, data[5] = %#x, data[6] = 0xFC\n",kAdtsHeader.data()[2],kAdtsHeader.data()[3],kAdtsHeader.data()[4],kAdtsHeader.data()[5]);
      }else if(useVideo){
	sp<AMessage> Am;
	extractor->getTrackFormat(trackIndex, &Am);

	sp<MetaData> meta;
	extractor->getSampleMeta(&meta);
	uint32_t sampleFlags = 0;
	int32_t val;
	
	if (meta->findInt32(kKeyIsSyncFrame, &val) && val != 0) {
	  int width , height;
	  Am->findInt32("width", &width);
	  Am->findInt32("height", &height);
	  bufferSize = width * height * 4;
	  
	  sp<ABuffer> sps, pps;
	  Am->findBuffer("csd-0", &sps);
	  Am->findBuffer("csd-1", &pps);;
	  
	  fwrite(sps->data(), 1, sps->size(), fp_out);
	  fwrite(pps->data(), 1, pps->size(), fp_out);
	  printf("trackIndex = %zu\n",trackIndex);
	  
	  print_str2hex((unsigned char*)"video sps", sps->data(), sps->size());
	  print_str2hex((unsigned char*)"video pps", pps->data(), pps->size());
	}
      }
      
      fwrite(newBuffer->data(), 1, newBuffer->size(), fp_out);

      int64_t timeUs;
      extractor->getSampleTime(&timeUs);

      sp<MetaData> meta;
      extractor->getSampleMeta(&meta);

      uint32_t sampleFlags = 0;
      if(useVideo)
	sampleFlags |= MediaCodec::BUFFER_FLAG_SYNCFRAME;

      printf("trackIndex = %zu, trv = %ld\n",trackIndex,trackIndexMap.valueFor(trackIndex));
      muxer->writeSampleData(newBuffer, trackIndexMap.valueFor(trackIndex), timeUs, sampleFlags);
      extractor->advance();
    }
  }

  muxer->stop();
  newBuffer.clear();

  fclose(fp_out);
  return 0;
}

int main(int argc, char **argv) {
  bool useAudio = false;
  bool useVideo = false;
  char *outputFileName = NULL;
  int rotationDegrees = 0;

  if(argc < 3){
    printf("usage: ./muxer  input_file  ouput_file   enable_audio:(0,1)   enable_video:(0,1)   rotation:(0-360) container:(default mp4 format).\n");
    return -1;
  }
  
  sp<ALooper> looper = new ALooper;
  looper->start();

  char *input_file = argv[1];
  char *output_file = argv[2];
  useAudio = atoi(argv[3]);
  useVideo = atoi(argv[4]);

  int result = muxing(input_file, useAudio, useVideo, output_file, rotationDegrees, MediaMuxer::OUTPUT_FORMAT_MPEG_4);
  looper->stop();
  return result;
}
