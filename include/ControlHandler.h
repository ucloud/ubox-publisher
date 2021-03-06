#ifndef _CONTROL_HANDLER_H_20210908_
#define _CONTROL_HANDLER_H_20210908_

#include "MediaStream.h"

class ControlHandler {
public:
    ControlHandler();
    ~ControlHandler();

    int StartStream(const char *inputType, const char* deviceName, const char* accel, int srcWidth, int srcHeight,
                    const char *encode, const char *decode, int dstWidth, int dstHeight, int fps, int limitfps, int bitrate,
                    const char* rtmpURL, bool clockEnable);
    int StopStream(const char* deviceName);
    int SetBitrate(const char* deviceName, int bitrate);
    int GetBitrate(const char* deviceName);
    int GetStatus(std::string &msg);

private:
    std::string devName;
    std::string rtmpURL;


    MediaStream* stream;

    std::mutex streamMutex;
    bool streamOpened;
};

#endif//_CONTROL_HANDLER_H_20210908_
