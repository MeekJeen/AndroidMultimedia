/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-14 12:29:05 星期四
* Filename      : aac_muxer_mp4.cpp
* Description   : MediaMuxer将aac封装为mp4格式示例
************************************************************/

#include <iostream>
#include <vector>
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
#include <binder/ProcessState.h>
#include <media/IMediaHTTPService.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaMuxer.h>

using namespace android;

std::vector<size_t> aacFrameLengths;
std::FILE *fp_input;
std::FILE  *fp_output;

void print_str2hex(int index, std::string name, unsigned char* str, size_t size){
  printf("%s[%d]: \n",name.c_str(),index);
  for (size_t i = 0; i < size; i++) {
    printf("%02x ",static_cast<unsigned char>(str[i]));
    if(i != 0 && i % 16 == 0)
      printf("\n");
  }
  printf("\n");
}

void getFrameLength(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);  
  char header[7];
  while(1){
    file.read(header, sizeof(header));
    if(file.eof())
      break;

    int frameLength = ((header[3] & 0x03) << 11) | (header[4] << 3) | ((header[5] & 0xE0) >> 5);//1110 0000
    if(frameLength>0){
      aacFrameLengths.push_back(frameLength);      
    }
    
    file.seekg(frameLength - sizeof(header), std::ios::cur);
  }
  file.close();
}

void set_aac_csd0(sp<AMessage> format){
  int profile = 1;
  int Sampleing_frequencry_index = 4;
  int channel = 2;
  uint8_t csd[2];

  csd[0] = ((profile + 1) << 3) | (Sampleing_frequencry_index >> 1);
  csd[1] = ((Sampleing_frequencry_index << 7) & 0x80) | (channel << 3);
  sp<ABuffer> audio_csd0 = new ABuffer(2);
  memcpy(audio_csd0->data(), csd, 2);  
  format->setBuffer("csd-0", audio_csd0);
}

int main(int argc, char *argv[]) {
  if(argc < 3){
    printf("usage:./aac_muxer_to_mp4 input.aac  ouput.mp4\n");
    return -1;
  }
  
  getFrameLength(argv[1]);
  fp_input = fopen(argv[1], "r");
  fp_output = fopen(argv[2], "w+");

  sp<MediaMuxer> muxer = new MediaMuxer(fileno(fp_output), MediaMuxer::OUTPUT_FORMAT_MPEG_4);
  sp<AMessage> format = new AMessage();
  format->setString(KEY_MIME, "audio/mp4a-latm");
  format->setInt32(KEY_SAMPLE_RATE, 44100);
  format->setInt32(KEY_CHANNEL_COUNT, 2);
  set_aac_csd0(format);

  ssize_t newTrackIndex = muxer->addTrack(format);
  muxer->start();

  int bufferSize = 100;
  std::vector<unsigned char> AacBuf(500);
  int64_t timeUs = 0;
  uint32_t sampleFlags = 0;
  int readsize = 0;

  for (int i = 0; i < aacFrameLengths.size(); i++) {
    AacBuf.resize(aacFrameLengths[i]);
    sp<ABuffer> newBuffer = new ABuffer(AacBuf.size());    
    readsize = fread(AacBuf.data(), 1, AacBuf.size(), fp_input);
    if (readsize <= 0){
      printf("H264 data buffer input End of Stream.\n");
      break;
    }

    newBuffer->setRange(newBuffer->offset(), AacBuf.size());
    memcpy(newBuffer->data(), AacBuf.data(), readsize);

    sampleFlags = MediaCodec::BUFFER_FLAG_SYNCFRAME;
    muxer->writeSampleData(newBuffer, newTrackIndex, timeUs, sampleFlags);
  }

  fclose(fp_input);
  fclose(fp_output);
  return 0;
}
