#ifndef _H264_ENCODER_H_
#define _H264_ENCODER_H_

#include <cstdint>
#include <cstring>
#include <utility>

extern "C" {
#include <x264.h>
};

class H264Encoder {
public:
    H264Encoder(int width, int height, int fps = 20, int bitrate = 256, int keyint = 10, double coeff = 1.5);
    ~H264Encoder();
    std::pair<int, char*> getMetadata();
    std::pair<int, char*> encode(char* frame, int picID);
    void ResetConstant(float con);
    void SetConstant(float con);
     void SetBitrate(int bitrate);
private:
    int mLumaSize;
    int mChromaSize;
    x264_param_t* x264_param;
    x264_t *mHandle;
    x264_nal_t *mNal;
    x264_picture_t mPicture;
    int mConn;
    double coeff;
};

#endif
