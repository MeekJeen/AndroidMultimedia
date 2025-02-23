/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-01 15:20:29 星期五
* Filename      : h264_muxer_mp4.cpp
* Description   : MediaMuxer将h264封装成mp4格式示例
************************************************************/

#include <cstring>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
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
using namespace std;

int mindex = 0;
std::vector<size_t> frameLengths;
std::vector<std::vector<uint8_t>> frameDatas;

int64_t getNowUs(){
  timeval tv;
  gettimeofday(&tv, 0);
  return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

void print_str2hex(int index, string name, unsigned char* str, size_t size){
  printf("%s[%d]: \n",name.c_str(),index);
  for (size_t i = 0; i < size; i++) {
    printf("%02x ",static_cast<unsigned char>(str[i]));
    if(i != 0 && i % 16 == 0)
      printf("\n");
  }
  printf("\n");
}

void parseH264File(const std::string& filename){
  std::ifstream file(filename, std::ios::binary);  
  std::vector<uint8_t> data(std::istreambuf_iterator<char>(file), {});
  std::vector<uint8_t> startCode1 = { 0x00, 0x00, 0x00, 0x01 };
  std::vector<uint8_t> startCode2 = { 0x00, 0x00, 0x01 };
  std::vector<std::string> frameTypes;
  size_t index = 0;

  while (index < data.size() - startCode1.size()){
    if (std::equal(startCode1.begin(), startCode1.end(), data.begin() + index) || std::equal(startCode2.begin(), startCode2.end(), data.begin() + index)){      
      size_t frameStart = index;
      size_t frameEnd = 0;
      std::string frameType;
      
      size_t nextStartCode1 = data.size();
      auto it1 = std::search(data.begin() + index + startCode1.size(), data.end(), startCode1.begin(), startCode1.end());
      if (it1 != data.end())
	nextStartCode1 = std::distance(data.begin(), it1);

      size_t nextStartCode2 = data.size();
      auto it2 = std::search(data.begin() + index + startCode2.size(), data.end(), startCode2.begin(), startCode2.end());
      if (it2 != data.end())
	nextStartCode2 = std::distance(data.begin(), it2);

      if (nextStartCode1 < nextStartCode2){
	frameEnd = nextStartCode1 - 1;
	frameType = "Unknown";
	if (data[frameStart + startCode1.size()] == 0x67)
	  frameType = "SPS";	  
	else if (data[frameStart + startCode1.size()] == 0x68)
	  frameType = "PPS";	
	else if (data[frameStart + startCode1.size()] == 0x06 || data[frameStart + startCode2.size()] == 0x06)
	  frameType = "SEI";	
	else if (data[frameStart + startCode1.size()] == 0x65 || data[frameStart + startCode2.size()] == 0x65)
	  frameType = "IDR (I-frame)";
	else if (data[frameStart + startCode1.size()] == 0x41)
	  frameType = "P-frame";
	else if (data[frameStart + startCode1.size()] == 0x01)
	  frameType = "B-frame";
      }else{
	frameEnd = nextStartCode2 - 1;
	frameType = "Unknown";
	if (data[frameStart + startCode1.size()] == 0x67)
	  frameType = "SPS";
	else if (data[frameStart + startCode1.size()] == 0x68)
	  frameType = "PPS";
	else if (data[frameStart + startCode1.size()] == 0x06 || data[frameStart + startCode2.size()] == 0x06)
	  frameType = "SEI";
	else if (data[frameStart + startCode1.size()] == 0x65 || data[frameStart + startCode2.size()] == 0x65)
	  frameType = "IDR (I-frame)";
	else if (data[frameStart + startCode1.size()] == 0x41)
	  frameType = "P-frame";
	else if (data[frameStart + startCode1.size()] == 0x01)
	  frameType = "B-frame";
      }

      size_t frameLength = frameEnd - frameStart + 1;      
      frameLengths.push_back(frameLength);
      frameTypes.push_back(frameType);
      
      std::vector<uint8_t> signal_frame(frameLengths[mindex]);
      memcpy(signal_frame.data(), data.data()+index, frameLengths[mindex]);
      frameDatas.push_back(signal_frame);
      mindex++;

      index = frameEnd;
    }
    index++;
  }
}

int main() {
  parseH264File("/data/debug/out_640x480.h264");
  FILE* inputFile = fopen("/data/debug/out_640x480.h264", "r");
  FILE *fp_out = fopen("/data/debug/output.mp4","w+");

  size_t bufferSize = 640 * 480 * 3 / 2 ;
  std::vector<unsigned char> buffer(bufferSize);
  ssize_t bytesRead;
  int64_t timeUs = 1000 * 1000;//20ms
  uint32_t sampleFlags = 0;

  sp<MediaMuxer> muxer = new MediaMuxer(fileno(fp_out), MediaMuxer::OUTPUT_FORMAT_MPEG_4);
  sp<AMessage> format = new AMessage();
  format->setString("mime", "video/avc");
  format->setInt32("width", 640);
  format->setInt32("height", 480);  
  
  sp<ABuffer> sps = new ABuffer(frameLengths[0]);
  sp<ABuffer> pps = new ABuffer(frameLengths[1]);

  memcpy(sps->data(), frameDatas[0].data(), frameLengths[0]);
  memcpy(pps->data(), frameDatas[1].data(), frameLengths[1]);

  format->setBuffer("csd-0", sps);
  format->setBuffer("csd-1", pps);

  print_str2hex(0, "video---- sps", frameDatas[0].data(), frameLengths[0]);
  print_str2hex(1, "video---- pps", frameDatas[1].data(), frameLengths[1]);

  ssize_t newTrackIndex = muxer->addTrack(format);
  muxer->setOrientationHint(0);
  muxer->start();

  sp<ABuffer> newBuffer = new ABuffer(bufferSize);
  bool mflag = false;

  timeUs = 0;  
  for (int i = 0; i < frameLengths.size(); i++) {
    buffer.resize(frameLengths[i]);
    bytesRead = fread(buffer.data(), 1, buffer.size(), inputFile);
    if(bytesRead <= 0)
      break;
    
    newBuffer->setRange(newBuffer->offset(),buffer.size());
    memcpy(newBuffer->data(), buffer.data(), buffer.size());

    if(memcmp(buffer.data(), "\x00\x00\x00\x01\x67",5) == 0 ||
       memcmp(buffer.data(), "\x00\x00\x01\x68",5) == 0     ||
       memcmp(buffer.data(), "\x00\x00\x00\x01\x6",5) == 0  ||
       memcmp(buffer.data(), "\x00\x00\x00\x01\x68",5) == 0 ||
       memcmp(buffer.data(), "\x00\x00\x01\x68",4) == 0     ||
       memcmp(buffer.data(), "\x00\x00\x01\x6",4) == 0){
      sampleFlags = 0;
    }else {      
      sampleFlags = MediaCodec::BUFFER_FLAG_SYNCFRAME;
    }

    muxer->writeSampleData(newBuffer, newTrackIndex, timeUs, sampleFlags);
    timeUs += (1000 * 1000 / 25);
  }
  
  fclose(inputFile);
  fclose(fp_out);
  muxer->stop();
  newBuffer.clear();
  return 0;
}
