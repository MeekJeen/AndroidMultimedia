/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-13 23:18:59 星期三
* Filename      : mp4_extract_aac.cpp
* Description   : NuMediaExtractor将mp4解封装为h264格式示例
************************************************************/
#include <iostream>
#include <media/IMediaHTTPService.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/NuMediaExtractor.h>

using namespace android;

void print_str2hex(unsigned char *name, unsigned char* str, size_t size){
  printf("%s: ",name);
  for (size_t i = 0; i < size; i++) {
    printf("%02x ", static_cast<unsigned char>(str[i]));
  }
  printf("\n");
}

void mp4_extract_h264(const char *inputFileName, const char *outputFileName) {
  sp<AMessage> format;

  FILE *fp_out = fopen(outputFileName,"w+");
  sp<NuMediaExtractor> extractor = new NuMediaExtractor(NuMediaExtractor::EntryPoint::OTHER);
  
  extractor->setDataSource(NULL, inputFileName);
  size_t trackCount = extractor->countTracks();
  size_t bufferSize = 1 * 1024 * 1024;

  for (size_t i = 0; i < trackCount; ++i){
    extractor->getTrackFormat(i, &format);
    printf("extractor getTrackFormat: trackindex[%zu] = %s\n",i,format->debugString().c_str());

    AString mime;
    format->findString("mime", &mime);
    printf("mime = %s\n",mime.c_str());

    bool isVideo = !strncasecmp(mime.c_str(), "video/", 6);

    if (isVideo) {
      extractor->selectTrack(i);
      break;
    }
  }

  int64_t muxerStartTimeUs = ALooper::GetNowUs();
  bool sawInputEOS = false;

  size_t trackIndex = -1;
  sp<ABuffer> newBuffer = new ABuffer(bufferSize);

  while (sawInputEOS == false) {
    status_t err = extractor->getSampleTrackIndex(&trackIndex);
    if (err != OK) {
      printf("saw input eos, err %d\n", err);
      sawInputEOS = true;
    } else if(trackIndex >= 0){
      extractor->readSampleData(newBuffer);

      {
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
      extractor->advance();
    }
  }

  fclose(fp_out);
}

int main(int argc, char **argv) {
  if(argc < 3){
    printf("usage: ./mp4_extract_h264  input.mp4  ouput.h264\n");
    return -1;
  }
  char *input_file = argv[1];
  char *output_file = argv[2];

  mp4_extract_h264(input_file, output_file);  
  return 0;
}
