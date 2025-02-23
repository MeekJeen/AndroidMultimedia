/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:51:36 星期六
* Filename      : h264_decode_yuv420p.cpp
* Description   : MediaCodec将h264解码为nv12格式示例
************************************************************/

#include <cstddef>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <binder/Parcel.h>
#include <mediadrm/ICrypto.h>
#include <binder/IPCThreadState.h>
#include <gui/Surface.h>
#include <media/MediaCodecBuffer.h>
#include <media/MediaCodecInfo.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <binder/ProcessState.h>
#include <media/IMediaHTTPService.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaMuxer.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/NuMediaExtractor.h>
#include <media/stagefright/MetaDataBase.h>

using namespace android;

static int64_t waitTime = 5000ll;//5ms

std::vector<size_t> frameLengths;

void parseH264File(const std::string& filename){
  std::ifstream file(filename, std::ios::binary);

  std::vector<uint8_t> data(std::istreambuf_iterator<char>(file), {});
  std::vector<uint8_t> startCode1 = { 0x00, 0x00, 0x00, 0x01 };
  std::vector<uint8_t> startCode2 = { 0x00, 0x00, 0x01 };
  std::vector<std::string> frameTypes;
  size_t index = 0;

  while (index < data.size() - startCode1.size()){  // 或者修改为 data.size() - startCode2.size()
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

      index = frameEnd;
    }
    index++;
  }
  
  for (size_t i = 0; i < frameLengths.size(); i++){
    std::cout << "Frame " << i + 1 << " - Type: " << frameTypes[i] << ", Length: " << frameLengths[i] << " bytes" << std::endl;
  }
}

typedef struct  {
  FILE                 *fp_input;
  FILE                 *fp_output;

  sp<MediaCodec>       codec;
  Vector<sp<MediaCodecBuffer> > inBuffers;
  Vector<sp<MediaCodecBuffer> > outBuffers;
  int                  width;
  int                  height;
  std::string          mime;
} Encoder;

void print_str2hex(unsigned char *name, unsigned char* str, size_t size){
  printf("%s: ",name);
  for (size_t i = 0; i < size; i++) {
    printf("%02x ", static_cast<unsigned char>(str[i]));
  }
  printf("\n");
}

status_t SetupCodec(Encoder *data) {
  sp<ALooper> looper = new ALooper;
  looper->setName("encode_looper");
  looper->start();

  sp<MediaCodec> codec = MediaCodec::CreateByType(looper, data->mime.c_str(), false);

  sp<AMessage> format = new AMessage;
  format->setInt32("width", data->width);
  format->setInt32("height", data->height);
  format->setString("mime", data->mime.c_str());

  printf("configure output format is '%s'\n", format->debugString(0).c_str());
  
  codec->configure(format, NULL, NULL, false);
  codec->start();
  codec->getInputBuffers(&data->inBuffers);
  codec->getOutputBuffers(&data->outBuffers);
  printf("got %zu input and %zu output buffers\n",data->inBuffers.size(), data->outBuffers.size());
  data->codec = codec;
  return OK;
}

status_t H264_decode_YUV(Encoder *data) {
  status_t ret;
  int h264size = 0, readsize = 0;
  size_t index;
  bool InputEOS = false;
  std::vector<unsigned char> H264Buf(h264size);

  for (int i = 0; i < frameLengths.size(); i++) {
    if (InputEOS == false) {
      h264size = frameLengths[i];
      H264Buf.resize(h264size);
      readsize = fread(H264Buf.data(), 1, H264Buf.size() , data->fp_input);

      if (readsize <= 0) {
	printf("H264 data buffer input End of Stream.\n");
	InputEOS = true;
      }
      
      ret = data->codec->dequeueInputBuffer(&index, waitTime);
      if (ret == OK) {	
	const sp<MediaCodecBuffer> buffer = data->inBuffers.itemAt(index);	
	memcpy(buffer->data(), H264Buf.data(), readsize);        
	data->codec->queueInputBuffer(index, 0, buffer->size(), waitTime, 0);
      }
    } else {
      ret = data->codec->dequeueInputBuffer(&index, waitTime);
      if (ret == OK) {	
	data->codec->queueInputBuffer(index, 0, 0,0, MediaCodec::BUFFER_FLAG_EOS);
      }
    }

    size_t offset, size;
    int64_t presentationTimeUs;
    uint32_t flags;
    
    ret = data->codec->dequeueOutputBuffer(&index, &offset, &size, &presentationTimeUs, &flags, waitTime);
    if(ret == OK) {      
      const sp<MediaCodecBuffer> buffer = data->outBuffers.itemAt(index);      
      fwrite(buffer->data(), 1, buffer->size(), data->fp_output);
      data->codec->releaseOutputBuffer(index);
    }

    if (flags == MediaCodec::BUFFER_FLAG_EOS) {
      printf("H264 decode success.\n");
      break;
    }
  }
  
  sp<AMessage> format;
  data->codec->getOutputFormat(&format);
  printf("INFO_FORMAT_CHANGED: %s\n", format->debugString().c_str());
  return 0;
}

int main() {  
  std::string filename = "/data/debug/out_640x480.h264";
  parseH264File(filename);

  Encoder ed;
  ed.fp_input = fopen("/data/debug/out_640x480.h264", "r");
  ed.fp_output = fopen("/data/debug/out_640_480.yuv", "w+");
  ed.width = 640;
  ed.height = 480;
  ed.mime = "video/avc";
  
  SetupCodec(&ed);
  H264_decode_YUV(&ed);

  ed.codec->stop();
  ed.codec->release();
  fclose(ed.fp_input);
  fclose(ed.fp_output);
  return 0;
}
