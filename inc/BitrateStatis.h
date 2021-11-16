#ifndef _BITRATE_STATIS_H_20210909_
#define _BITRATE_STATIS_H_20210909_

class BitrateStatis{
public:
    BitrateStatis(int fps = 30);
    ~BitrateStatis();

    void Init(int fps);
    void PushDataSize(int size);
    int GetLastBitrate() {return bitrate_last*fps/(128*total_last);}
    int GetLast1SBitrate() {return bitrate_1s*fps/(128*total_1s);}
    int GetLast3SBitrate() {return bitrate_3s*fps/(128*total_3s);}
    int GetLast5SBitrate() {return bitrate_5s*fps/(128*total_5s);}
    int GetLast10SBitrate() {return bitrate_10s*fps/(128*total_10s);}
    int GetLast30SBitrate() {return bitrate_30s*fps/(128*total_30s);}
    int GetLast60SBitrate() {return bitrate_60s*fps/(128*total_60s);}
    float GetConstant(int dstBitrate);

private:
    int fps;
    int head;
    int tail;
    int* buffer;
    int buffer_size;
    int bitrate_last;
    int bitrate_1s;
    int bitrate_3s;
    int bitrate_5s;
    int bitrate_10s;
    int bitrate_30s;
    int bitrate_60s;
    int total_last;
    int total_1s;
    int total_3s;
    int total_5s;
    int total_10s;
    int total_30s;
    int total_60s;
    int data_last;
    int data_1s;
    int data_3s;
    int data_5s;
    int data_10s;
    int data_30s;
    int data_60s;
    float constant;
    bool bPush;

    static float ratio_map[100];
};

#endif//_BITRATE_STATIS_H_20210909_
