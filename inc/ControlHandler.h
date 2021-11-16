#ifndef _CONTROL_HANDLER_H_20210908_
#define _CONTROL_HANDLER_H_20210908_

#include "MediaStream.h"
#include "RTMPPublisher.h"

class ControlHandler {
public:
    ControlHandler();
    ~ControlHandler();

    int StartStream(const char* deviceName, int srcWidth, int srcHeight,
                    int dstWidth, int dstHeight, int fps, int bitrate,
                    const char* rtmpURL);
    int StopStream(const char* deviceName);
    int SetBitrate(const char* deviceName, int bitrate);
    int GetBitrate(const char* deviceName);

private:
    std::string devName;
    std::string rtmpURL;

    PacketQueue queue;
    MemoryPool pool;

    MediaStream* stream;
    RTMPPublisher* publisher;

    std::mutex streamMutex;
    bool streamOpened;
};

#endif//_CONTROL_HANDLER_H_20210908_
