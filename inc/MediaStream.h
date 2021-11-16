#ifndef _MEDIASTREAM_H_20210909_
#define _MEDIASTREAM_H_20210909_

#include <thread>
#include "ImageProcessor.h"
#include "BitrateStatis.h"
#include "H264Encoder.h"
#include "PacketQueue.h"
#include "MemoryPool.h"

class MediaStream {
public:
    MediaStream();
    ~MediaStream();

    int Open(const char* deviceName, int srcWidth, int srcHeight,
             int dstWidth, int dstHeight, int fps, int bitrate,
             PacketQueue *queue, MemoryPool *pool);
    int Close();

    void SetBitrate(int bitrate);
    int GetLast1SBitrate() {return statis.GetLast1SBitrate();}
    int GetLast3SBitrate() {return statis.GetLast3SBitrate();}
    int GetLast5SBitrate() {return statis.GetLast5SBitrate();}
    int GetLast10SBitrate() {return statis.GetLast10SBitrate();}
    int GetLast30SBitrate() {return statis.GetLast30SBitrate();}
    int GetLast60SBitrate() {return statis.GetLast60SBitrate();}

private:
    void run();

private:
    ImageProcessor imgProc;
    BitrateStatis statis;
    bool quit;
    bool opened;
    std::thread stringThread;
    std::mutex threadMutex;
    bool changeCon;

    PacketQueue *queue;
    MemoryPool *pool;
    std::string deviceName;
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    int fps;
    int bitrate;
    int picID;
};

#endif//_MEDIASTREAM_H_20210909_
