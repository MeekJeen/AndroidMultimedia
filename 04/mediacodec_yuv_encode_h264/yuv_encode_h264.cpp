/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-09-02 15:44:55 星期六
* Filename      : yuv_encode_h264.cpp
* Description   : MediaCodec将yuv420p(NV12)或yuv420sp(NV21)编码为h264示例
************************************************************/

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaErrors.h>
#include <media/openmax/OMX_IVCommon.h>
#include <mediadrm/ICrypto.h>
#include <binder/IPCThreadState.h>
#include <gui/Surface.h>
#include <media/MediaCodecBuffer.h>

using namespace android;
static int64_t waitTime = 500ll;//500us

typedef struct  {
  FILE                 *fp_input;
  FILE                 *fp_output;

  sp<MediaCodec>       codec;
  Vector<sp<MediaCodecBuffer> > inBuffers;
  Vector<sp<MediaCodecBuffer> > outBuffers;
  int                  width;
  int                  height;
  std::string          mime;
  int                  color_format;
  int                  bitrate;
  int                  frame_rate;
  int                  I_frame;
} Encoder;

status_t SetupCodec(Encoder *data){
  sp<ALooper> looper = new ALooper;
  looper->setName("native_enc_looper");
  looper->start();

  sp<MediaCodec> codec = MediaCodec::CreateByType(looper, data->mime.c_str(), true);
  sp<AMessage> format = new AMessage;
  format->setInt32("width", data->width);
  format->setInt32("height", data->height);
  format->setString("mime", data->mime.c_str());
  format->setInt32("color-format", data->color_format);
  format->setInt32("bitrate", data->bitrate);
  format->setFloat("frame-rate", data->frame_rate);
  format->setInt32("i-frame-interval", data->I_frame);

  printf("configure output format is '%s'\n", format->debugString(0).c_str());
  codec->configure(format, NULL, NULL,MediaCodec::CONFIGURE_FLAG_ENCODE);
  codec->start();
  codec->getInputBuffers(&data->inBuffers);
  codec->getOutputBuffers(&data->outBuffers);

  printf("got %zu input and %zu output buffers\n",data->inBuffers.size(), data->outBuffers.size());
  data->codec = codec;
  return OK;
}

status_t YUVToH264(Encoder *data) {
  status_t ret;
  int32_t yuvsize, readsize;
  size_t index;
  bool InputEOS = false;

  // YUV420SP input deault
  yuvsize = data->width * data->height * 3 / 2;
  std::vector<unsigned char> yuvBuf(yuvsize);

  int in_frame = 0;
  for (;;) {
    if (InputEOS == false) {
      readsize = fread(yuvBuf.data(), 1, yuvsize, data->fp_input);
      if (readsize <= 0) {
	printf("saw input eos\n");
	InputEOS = true;
      }

      int64_t timeUs = 0;      
      ret = data->codec->dequeueInputBuffer(&index, waitTime);
      if (ret == OK) {
	const sp<MediaCodecBuffer> &buffer = data->inBuffers.itemAt(index);
	memcpy(buffer->base(), yuvBuf.data(), readsize);
	ret = data->codec->queueInputBuffer(index, 0, buffer->size(), waitTime, 0);
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
      const sp<MediaCodecBuffer> &buffer = data->outBuffers.itemAt(index);
      fwrite(buffer->base(), 1, buffer->size(), data->fp_output);
      fflush(data->fp_output);
      data->codec->releaseOutputBuffer(index);
    }

    if (flags == MediaCodec::BUFFER_FLAG_EOS) {
      printf("H264 encode success!\n");
      break;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  if(argc != 3){
   printf("usage: ./yuv_to_h264 test.yuv output.h264\n");
   return -1;
  }

  Encoder ed;
  ed.fp_input = fopen(argv[1], "r");
  ed.fp_output = fopen(argv[2], "w+");
  ed.width = 1920;
  ed.height = 1080;
  ed.mime = "video/avc";
  ed.color_format = OMX_COLOR_FormatYUV420SemiPlanar;
  ed.bitrate = 4200 * 10000;
  ed.frame_rate = 30;
  ed.I_frame = 30;
  
  SetupCodec(&ed);
  YUVToH264(&ed);
  
  ed.codec->stop();
  ed.codec->release();
  fclose(ed.fp_input);
  fclose(ed.fp_output);
  return 0;
}
