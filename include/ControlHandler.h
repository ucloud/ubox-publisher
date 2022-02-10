#ifndef _CONTROL_HANDLER_H_20210908_
#define _CONTROL_HANDLER_H_20210908_

#include "MediaStream.h"

class ControlHandler {
public:
    ControlHandler();
    ~ControlHandler();

    int StartStream(const char *inputType, const char* deviceName, const char* accel, int srcWidth, int srcHeight,
                    bool copy, int dstWidth, int dstHeight, int fps, int limitfps, int bitrate,
                    const char* rtmpURL);
    int StopStream(const char* deviceName);
    int SetBitrate(const char* deviceName, int bitrate);
    int GetBitrate(const char* deviceName);

private:
    std::string devName;
    std::string rtmpURL;


    MediaStream* stream;

    std::mutex streamMutex;
    bool streamOpened;
};

#endif//_CONTROL_HANDLER_H_20210908_
