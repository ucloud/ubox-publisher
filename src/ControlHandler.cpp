#include "ControlHandler.h"
#include "tinylog/tlog.h"

ControlHandler::ControlHandler(){
    devName = "/dev/video0";
    rtmpURL = "";
    stream = NULL;
    publisher = NULL;
    streamOpened = false;
}

ControlHandler::~ControlHandler() {
    StopStream(devName.c_str());
}

int ControlHandler::StartStream(const char* deviceName, int srcWidth, int srcHeight,
                int dstWidth, int dstHeight, int fps, int bitrate, int keyint, double coeff,
                const char* rtmpURL) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (streamOpened) {
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

    if (publisher != NULL) {
        publisher->Close();
    } else {
        publisher = new RTMPPublisher();
    }

    int ret = stream->Open(deviceName, srcWidth, srcHeight, dstWidth, dstHeight, fps, bitrate, keyint, coeff, &queue, &pool);
    if (ret != 0) {
        tlog(TLOG_INFO, "stream open failed\n");
        streamOpened = false;
        delete stream;
        stream = NULL;
        delete publisher;
        publisher = NULL;
        lock.unlock();
        return -1;
    }


    this->rtmpURL = rtmpURL;
    //if (strcmp(deviceName, "/dev/video0") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo0";
    //} else if (strcmp(deviceName, "/dev/video1") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo1";
    //} else if (strcmp(deviceName, "/dev/video2") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo2";
    //} else if (strcmp(deviceName, "/dev/video3") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo3";
    //} else if (strcmp(deviceName, "/dev/video4") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo4";
    //} else if (strcmp(deviceName, "/dev/video5") == 0) {
    //    this->rtmpURL = "rtmp://106.75.32.7/live/haomo5";
    //}

    ret = publisher->Connect(this->rtmpURL.c_str(), &queue, &pool);
    if (ret != 0) {
        tlog(TLOG_INFO, "rtmp publisher open failed\n");
        streamOpened = false;
        stream->Close();
        delete stream;
        stream = NULL;
        delete publisher;
        publisher = NULL;
        lock.unlock();
        return -1;
    }
    lock.unlock();
    return 0;
}

int ControlHandler::StopStream(const char* deviceName) {
    std::unique_lock<std::mutex> lock(streamMutex);
    if (publisher != NULL) {
        publisher->Close();
        delete publisher;
        publisher = NULL;
    }
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
        int bitrate = stream->GetLast1SBitrate();
        lock.unlock();
        return bitrate;
    }
    lock.unlock();
    return 0;
}
