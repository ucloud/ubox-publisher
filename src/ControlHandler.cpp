#include "ControlHandler.h"
#include "tinylog/tlog.h"

ControlHandler::ControlHandler(){
    devName = "/dev/video0";
    rtmpURL = "";
    stream = NULL;
    streamOpened = false;
}

ControlHandler::~ControlHandler() {
    StopStream(devName.c_str());
}

int ControlHandler::StartStream(const char *inputType, const char* deviceName, const char *accel, int srcWidth, int srcHeight,
                const char *encode, const char *decode, int dstWidth, int dstHeight, int fps, int inputfps, int bitrate,
                const char* rtmpURL) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (streamOpened) {
        tlog(TLOG_INFO, "stream already opened");
        lock.unlock();
        return -1;
    }
    streamOpened = true;

    devName = deviceName;
    if (stream != NULL) {
        stream->Close();
    } else {
        stream = new MediaStream();
    }

    int ret = stream->Open(inputType, deviceName, accel, srcWidth, srcHeight, encode, decode, dstWidth, dstHeight, fps, inputfps, bitrate, rtmpURL);
    if (ret != 0) {
        tlog(TLOG_INFO, "stream open failed");
        streamOpened = false;
        delete stream;
        stream = NULL;
        lock.unlock();
        return -1;
    }


    this->rtmpURL = rtmpURL;
    lock.unlock();
    return 0;
}

int ControlHandler::StopStream(const char* deviceName) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (stream != NULL) {
        stream->Close();
        delete stream;
        stream = NULL;
    }
    streamOpened = false;
    lock.unlock();
    return 0;
}

int ControlHandler::SetBitrate(const char* deviceName, int bitrate) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (!streamOpened) {
        lock.unlock();
        return -1;
    }
    if (stream != NULL) {
        stream->SetBitrate(bitrate);
    }
    lock.unlock();
    return 0;
}

int ControlHandler::GetBitrate(const char* deviceName) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (!streamOpened) {
        lock.unlock();
        return 0;
    }
    if (stream != NULL) {
        int bitrate = 0;
        lock.unlock();
        return bitrate;
    }
    lock.unlock();
    return 0;
}
