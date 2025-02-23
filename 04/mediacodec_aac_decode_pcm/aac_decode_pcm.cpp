/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:51:36 星期六
* Filename      : aac_decode_pcm.cpp
* Description   : MediaCodec将aac解码为pcm格式示例
************************************************************/

#include <cstddef>
#include <cstdio>
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

std::vector<size_t> aacFrameLengths;
static int64_t waitTime = 5 * 1000ll;//5ms
std::FILE *fp_input;
std::FILE  *fp_output;

typedef struct  {
  sp<MediaCodec>       codec;
  Vector<sp<MediaCodecBuffer> > inBuffers;
  Vector<sp<MediaCodecBuffer> > outBuffers;
} AACEncoder;

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
  printf("csd[0] = %#x, csd[1] = %#x\n",audio_csd0->data()[0],audio_csd0->data()[1]);
  format->setBuffer("csd-0", audio_csd0);
}

void SetupAAcDecoder(AACEncoder *data) {
  sp<ALooper> looper = new ALooper;
  looper->setName("mediacodec_aac_decode_pcm");
  looper->start();

  sp<MediaCodec> codec = MediaCodec::CreateByType(looper, "audio/mp4a-latm", false);
  sp<AMessage> format = new AMessage;
  format->setString(KEY_MIME, "audio/mp4a-latm");
  format->setInt32(KEY_SAMPLE_RATE, 44100);
  format->setInt32(KEY_CHANNEL_COUNT, 2);
  format->setInt32(KEY_IS_ADTS, 1);
  set_aac_csd0(format);
  printf("configure output format is '%s'\n", format->debugString(0).c_str());
  
  codec->configure(format, NULL, NULL, false);
  codec->start();
  codec->getInputBuffers(&data->inBuffers);
  codec->getOutputBuffers(&data->outBuffers);
  printf("got %zu input and %zu output buffers\n",data->inBuffers.size(), data->outBuffers.size());
  data->codec = codec;
}

status_t aac_decode_pcm(AACEncoder *data, std::FILE *fp_input, std::FILE *fp_output) {
  status_t ret;
  int h264size = 0, readsize = 0;
  size_t index;
  bool InputEOS = false;
  std::vector<unsigned char> H264Buf(h264size);
  int aacReadLen = 0;

  for (int i = 0; i < aacFrameLengths.size(); i++) {
    if (InputEOS == false) {      
      std::vector<unsigned char> H264Buf(aacFrameLengths[i]);

      readsize = fread(H264Buf.data(), 1, H264Buf.size(), fp_input);
      if (readsize < 0) {
	printf("H264 data buffer input End of Stream.\n");
	InputEOS = true;
      }
      
      ret = data->codec->dequeueInputBuffer(&index, waitTime);
      if (ret == OK) {
	const sp<MediaCodecBuffer> buffer = data->inBuffers.itemAt(index);
	buffer->setRange(buffer->offset(), readsize);
	memcpy(buffer->data(), H264Buf.data(), H264Buf.size());
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
      fwrite(buffer->data(), 1, buffer->size(), fp_output);      
      data->codec->releaseOutputBuffer(index);
    }

    if (flags == MediaCodec::BUFFER_FLAG_EOS) {      
      break;
    }
  }
  
  sp<AMessage> format;
  data->codec->getOutputFormat(&format);
  printf("INFO_FORMAT_CHANGED: %s\n", format->debugString().c_str());
  return 0;
}

int main(int argc, char *argv[]) {
  if(argc < 3){
    printf("usage:./aac_decoder_pcm input.aac  ouput.pcm\n");
    return -1;
  }
  
  AACEncoder data;
  
  getFrameLength(argv[1]);
  fp_input = fopen(argv[1], "r");
  fp_output = fopen(argv[2], "w+");

  SetupAAcDecoder(&data);
  aac_decode_pcm(&data, fp_input, fp_output);

  fclose(fp_input);
  fclose(fp_output);
  data.codec->stop();
  data.codec->release();
  return 0;
}
