#include "MediaStream.h"
#include "V4L2Source.h"
#include "H264RTMPPackager.h"
#include "tinylog/tlog.h"

MediaStream::MediaStream() {
    queue = NULL;
    pool = NULL;
    opened = false;
    changeCon = true;
    picID = 0;
}

MediaStream::~MediaStream() {
    queue = NULL;
    pool = NULL;
}

int MediaStream::Open(const char *deviceName, int srcWidth, int srcHeight, int dstWidth,
                      int dstHeight, int fps, int bitrate,
                      PacketQueue *queue, MemoryPool *pool) {
    std::unique_lock<std::mutex> lock(threadMutex);
    if (opened) {
        tlog(TLOG_INFO, "media stream reopen\n");
        lock.unlock();
        return -1;
    }
    opened = true;
    lock.unlock();

    this->queue = queue;
    this->pool = pool;
    this->deviceName = deviceName;
    this->srcWidth = srcWidth;
    this->srcHeight = srcHeight;
    this->dstWidth = dstWidth;
    this->dstHeight = dstHeight;
    this->fps = fps;
    this->bitrate = bitrate;

    picID = 0;
    quit = false;
    stringThread = std::thread(&MediaStream::run, this);
    return 0;
}

int MediaStream::Close() {
    std::unique_lock<std::mutex> lock(threadMutex);
    quit = true;
    try {
        stringThread.join();
    } catch (...){
        tlog(TLOG_INFO, "catch string thread error!\n");
        tlog(TLOG_INFO, "string thread quit!\n");
        opened = false;
        lock.unlock();
        return 0;
    }
    tlog(TLOG_INFO, "string thread quit!\n");
    opened = false;
    lock.unlock();
    return 0;
}

void MediaStream::SetBitrate(int bitrate) {
    tlog(TLOG_INFO, "set bitrate %d\n", bitrate);
    if ( this->bitrate != bitrate) {
	changeCon = true;
    }
    this->bitrate = bitrate;
}

void MediaStream::run() {
    V4L2Source source(srcWidth, srcHeight, deviceName.c_str());
    //    source = new OpencvSource(srcWidth, srcHeight, deviceName);
    if (!source.isOpened()) {
        tlog(TLOG_INFO, "video device is not open\n");
        opened = false;
        return;
    }

    // prepare
    statis.Init(fps);
    H264Encoder encoder(dstWidth, dstHeight, fps, bitrate);
    H264RTMPPackager packager;
    RTMPPacket metaPkt;
    char metaBuf[256];

    // warm-up
    auto result = encoder.getMetadata();
    metaPkt = packager.metadata(metaBuf, result.second, result.first);
    source.getNextFrame(); // warm-up

    // start
    int frame_interval = 1000000/fps;
    const int adjust_interval = 100000;
    int frame_duration = 0;
    int adust_duration = 0;
    auto cur = std::chrono::system_clock::now();
    auto last = cur;
    bool keyFrame = false;
    tlog(TLOG_INFO, "h264 stream is running\n");

    while (!quit) {
        picID++;
        auto frame_start = std::chrono::system_clock::now();
        char* srcFrame = source.getNextFrame();
        //auto frame_end = std::chrono::system_clock::now();
        if (srcFrame == NULL) {
            tlog(TLOG_INFO, "no frame\n");
            usleep(1000);
            continue;
        }
        char* frame = (char*)imgProc.YUY2I420((unsigned char*)srcFrame, srcWidth, srcHeight, dstWidth, dstHeight);
        result = encoder.encode(frame, picID);
        //auto encode_end = std::chrono::system_clock::now();
        //tlog(TLOG_INFO, "frame id: %d, get start time %ld, get end time %ld, encode end time %ld\n", picID,
        tlog(TLOG_INFO, "frame id: %d, get start time %ld\n", picID,
                std::chrono::duration_cast<std::chrono::milliseconds>(frame_start.time_since_epoch()).count());
                //std::chrono::duration_cast<std::chrono::milliseconds>(frame_end.time_since_epoch()).count(),
                //std::chrono::duration_cast<std::chrono::milliseconds>(encode_end.time_since_epoch()).count());
        statis.PushDataSize(result.first);

        keyFrame = H264RTMPPackager::isKeyFrame(result.second);
        if (keyFrame) {
            if (!queue->push(metaPkt, true, keyFrame,
                        std::chrono::time_point_cast<std::chrono::milliseconds>(last).time_since_epoch().count())) {
                tlog(TLOG_INFO, "rtmp packet queue full\n");
                continue;
            }
            statis.PushDataSize(result.first);
        }

        char* buf = pool->getChunk(packager.getBodyLength(result.first));
        RTMPPacket framePkt = packager.pack(buf, result.second, result.first);
        if (!queue->push(framePkt, false, keyFrame,
                    std::chrono::time_point_cast<std::chrono::milliseconds>(last).time_since_epoch().count(), picID)) {
            pool->putChunk(packager.getBodyLength(result.first), buf);
            tlog(TLOG_INFO, "rtmp packet queue full\n");
            continue;
        }

        //if(adust_duration > adjust_interval && changeCon) {
        if(adust_duration > adjust_interval) {
            float constant = statis.GetConstant(bitrate);
            if (constant > 10) {
                encoder.SetConstant(constant);
		//changeCon = false;
            }
            adust_duration = 0;
        }

        cur = std::chrono::system_clock::now();
        frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(cur - last).count();
        adust_duration += frame_duration;

        if (frame_duration < frame_interval) {
            tlog(TLOG_INFO, ".....sleep....\n");
            usleep(frame_interval - frame_duration);
        }

        last = cur;
    }
    tlog(TLOG_INFO, "h264 stream is stopped\n");
}
