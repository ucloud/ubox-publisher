#include "H264Encoder.h"
#include <iostream>
#include "tinylog/tlog.h"


H264Encoder::H264Encoder(int width, int height, int fps, int bitrate, int keyint, double coeff){
    mLumaSize = width * height;
    mChromaSize = mLumaSize / 4;
    mConn = 31;

    x264_param = new x264_param_t;
    x264_param_default_preset(x264_param, "ultrafast", "zerolatency");
    x264_param_apply_profile(x264_param, "baseline");
    x264_param->i_log_level = X264_LOG_ERROR;
    x264_param->i_csp = X264_CSP_I420;
    x264_param->i_width = width;
    x264_param->i_height = height;
    x264_param->i_fps_den = 1;
    x264_param->i_fps_num = fps;
    x264_param->i_keyint_max = keyint;
    x264_param->i_timebase_num = 1;
    x264_param->i_timebase_den = fps;
    x264_param->rc.i_rc_method = X264_RC_ABR;
    x264_param->rc.i_bitrate = bitrate;
    x264_param->rc.i_vbv_max_bitrate = coeff*bitrate;
    x264_param->rc.i_vbv_buffer_size = 2*bitrate;
    //x264_param->rc.i_rc_method = X264_RC_CRF;
    //x264_param->rc.f_rf_constant = mConn;
    x264_param->b_repeat_headers = 0; // add sps and pps manually
    x264_param->b_annexb = 0; // for the convenience of packing

    tlog(TLOG_INFO, "encoder,fps(%d), bitrate( %d), keyint(%d), coeff(%f)\n", fps, bitrate, keyint, coeff);

    x264_picture_alloc(&mPicture, x264_param->i_csp, x264_param->i_width, x264_param->i_height);
    mHandle = x264_encoder_open(x264_param);
}

H264Encoder::~H264Encoder() {
    x264_encoder_close(mHandle);
    x264_picture_clean(&mPicture);
}

std::pair<int, char*> H264Encoder::encode(char* frame, int picID) {
    int temp, size;
    x264_picture_t out;

    memcpy(mPicture.img.plane[0], frame, mLumaSize);
    memcpy(mPicture.img.plane[1], frame + mLumaSize, mChromaSize);
    memcpy(mPicture.img.plane[2], frame + mLumaSize + mChromaSize, mChromaSize);

    mPicture.i_pts = picID;

    size = x264_encoder_encode(mHandle, &mNal, &temp, &mPicture, &out);

    tlog(TLOG_INFO, "encode con: %f\n", x264_param->rc.f_rf_constant);
    return std::make_pair(size, reinterpret_cast<char*>(mNal->p_payload));
}

std::pair<int, char*> H264Encoder::getMetadata() {
    int temp, size;

    x264_encoder_headers(mHandle, &mNal, &temp);
    size = mNal[0].i_payload + mNal[1].i_payload;

    return std::make_pair(size, reinterpret_cast<char*>(mNal->p_payload));
}

void H264Encoder::ResetConstant(float con) {
    tlog(TLOG_INFO, "reset constant,cur: %f, dst: %f\n", x264_param->rc.f_rf_constant, con);
    float dst_con = x264_param->rc.f_rf_constant + con;
    if(dst_con > 51) {
        x264_param->rc.f_rf_constant  = 51;
    } else if (dst_con < 0) {
        x264_param->rc.f_rf_constant  = 0;
    } else {
        x264_param->rc.f_rf_constant  = dst_con;
    }
    x264_encoder_reconfig(mHandle, x264_param);
}

void H264Encoder::SetConstant(float con) {
    tlog(TLOG_INFO, "reset constant,cur: %f, dst: %f\n", x264_param->rc.f_rf_constant, con);
    if (mConn == (int)con) {
        return;
    }
    mConn = (int)con;
    if(mConn > 51) {
        x264_param->rc.f_rf_constant  = 51;
    } else if (mConn < 0) {
        x264_param->rc.f_rf_constant  = 0;
    } else {
        x264_param->rc.f_rf_constant  = mConn;
    }
    x264_encoder_reconfig(mHandle, x264_param);
}

void H264Encoder::SetBitrate(int bitrate) {
    tlog(TLOG_INFO, "reset bitrate,cur: %f, dst: %f\n", x264_param->rc.i_bitrate, bitrate);
    x264_param->rc.i_bitrate = bitrate;
    x264_param->rc.i_vbv_max_bitrate = bitrate;
    x264_param->rc.i_vbv_buffer_size = 2*bitrate;
    x264_encoder_reconfig(mHandle, x264_param);
}
