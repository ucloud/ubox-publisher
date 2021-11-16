#include "BitrateStatis.h"
#include <stdio.h>

float BitrateStatis::ratio_map[100] = {
        0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.6, 0.7, 0.8,
        0.9, 0.9, 1.0, 1.1, 1.2, 1.2, 1.3, 1.4, 1.5, 1.5,
        1.6, 1.7, 1.7, 1.8, 1.9, 2.0, 2.0, 2.1, 2.2, 2.2,
        2.3, 2.4, 2.4, 2.5, 2.5, 2.6, 2.7, 2.7, 2.8, 2.9,
        2.9, 3.0, 3.0, 3.1, 3.2, 3.2, 3.3, 3.3, 3.4, 3.5,
        3.5, 3.6, 3.6, 3.7, 3.7, 3.8, 3.9, 3.9, 4.0, 4.0,
        4.1, 4.1, 4.2, 4.2, 4.3, 4.3, 4.4, 4.4, 4.5, 4.5,
        4.6, 4.6, 4.7, 4.7, 4.8, 4.8, 4.9, 4.9, 5.0, 5.0,
        5.1, 5.1, 5.2, 5.2, 5.3, 5.3, 5.4, 5.4, 5.5, 5.5,
        5.6, 5.6, 5.6, 5.7, 5.7, 5.8, 5.8, 5.9, 5.9, 5.9
};

BitrateStatis::BitrateStatis(int fps) {
    this->fps = fps;
    buffer_size = fps*60;
    buffer = new int[buffer_size]();
    head = 0;
    tail = buffer_size - 1;
    bitrate_last = bitrate_1s = bitrate_3s = bitrate_5s = bitrate_10s = bitrate_30s = bitrate_60s = 0;
    total_last = 10;
    total_1s = fps;
    total_3s = 3*fps;
    total_5s = 5*fps;
    total_10s = 10*fps;
    total_30s = 30*fps;
    total_60s = 60*fps;
    data_last = (head+total_last-1)%buffer_size;
    data_1s = (head+total_1s-1)%buffer_size;
    data_3s = (head+total_3s-1)%buffer_size;
    data_5s = (head+total_5s-1)%buffer_size;
    data_10s = (head+total_10s-1)%buffer_size;
    data_30s = (head+total_30s-1)%buffer_size;
    data_60s = (head+total_60s-1)%buffer_size;
    constant = 23;
    bPush = false;
}

void BitrateStatis::Init(int fps) {
    if (buffer != NULL) {
        delete [] buffer;
        buffer = NULL;
    }
    this->fps = fps;
    buffer_size = fps*60;
    buffer = new int[buffer_size]();
    head = 0;
    tail = buffer_size - 1;
    bitrate_last = bitrate_1s = bitrate_3s = bitrate_5s = bitrate_10s = bitrate_30s = bitrate_60s = 0;
    total_last = 10;
    total_1s = fps;
    total_3s = 3*fps;
    total_5s = 5*fps;
    total_10s = 10*fps;
    total_30s = 30*fps;
    total_60s = 60*fps;
    data_last = (head+total_last-1)%buffer_size;
    data_1s = (head+total_1s-1)%buffer_size;
    data_3s = (head+total_3s-1)%buffer_size;
    data_5s = (head+total_5s-1)%buffer_size;
    data_10s = (head+total_10s-1)%buffer_size;
    data_30s = (head+total_30s-1)%buffer_size;
    data_60s = (head+total_60s-1)%buffer_size;
    constant = 23;
    bPush = false;
}

BitrateStatis::~BitrateStatis() {
    delete [] buffer;
    buffer = NULL;
}

void BitrateStatis::PushDataSize(int size) {
    int tmp_size = size;
    bitrate_last = bitrate_last - buffer[data_last] +tmp_size;
    bitrate_1s = bitrate_1s - buffer[data_1s] +tmp_size;
    bitrate_3s = bitrate_3s - buffer[data_3s] +tmp_size;
    bitrate_5s = bitrate_5s - buffer[data_5s] +tmp_size;
    bitrate_10s = bitrate_10s - buffer[data_10s] +tmp_size;
    bitrate_30s = bitrate_30s - buffer[data_30s] +tmp_size;
    bitrate_60s = bitrate_60s - buffer[data_60s] +tmp_size;
    tail = (tail + buffer_size - 1)%buffer_size;
    head = (head + buffer_size - 1)%buffer_size;
    buffer[head] = tmp_size;
    data_last = (head+total_last-1)%buffer_size;
    data_1s = (head+total_1s-1)%buffer_size;
    data_3s = (head+total_3s-1)%buffer_size;
    data_5s = (head+total_5s-1)%buffer_size;
    data_10s = (head+total_10s-1)%buffer_size;
    data_30s = (head+total_30s-1)%buffer_size;
    data_60s = (head+total_60s-1)%buffer_size;
    bPush = true;
}

float BitrateStatis::GetConstant(int dstBitrate) {
    if (!bPush) {
	return constant;    
    }
    bPush = false;
    if (dstBitrate <= 0)
        return 0;
    int bitrate = GetLast1SBitrate();
    int bitrate_cur = GetLastBitrate();
    if(dstBitrate > bitrate) {
        if (dstBitrate < bitrate_cur)
            return 0;
        unsigned int ratio = (dstBitrate-bitrate)*100/bitrate;
        if(ratio < 10)
            return 0;
        else if(ratio < 100) {
            //tlog(TLOG_INFO, "dst: %d, cur: %d, ratio: %d, con: %f\n", mBitrate, bitrate, ratio, -ratio_map[ratio]);
            constant -= ratio_map[ratio];
        } else {
            //tlog(TLOG_INFO, "dst: %d, cur: %d, ratio: %d, con: %f\n", mBitrate, bitrate, ratio, -6.0);
            constant -= 6;
        }
    }
    else {
        if (dstBitrate > bitrate_cur)
            return 0;
        unsigned int ratio = (bitrate-dstBitrate)*100/dstBitrate;
        if(ratio < 10)
            return 0;
        else if(ratio < 100) {
            //tlog(TLOG_INFO, "dst: %d, cur: %d, ratio: %d, con: %f\n", mBitrate, bitrate, ratio, ratio_map[ratio]);
            constant += ratio_map[ratio];
        } else {
            //tlog(TLOG_INFO, "dst: %d, cur: %d, ratio: %d, con: %f\n", mBitrate, bitrate, ratio, 6.0);
            constant += 6;
        }
    }
    if (constant >= 51)
        constant = 51;
    if (constant <= 11)
        constant = 11;
    return constant;
}
