#ifndef _RTMP_PUBLISHER_H_
#define _RTMP_PUBLISHER_H_

#include <librtmp/rtmp.h>
#include <thread>
#include <mutex>
#include "PacketQueue.h"
#include "MemoryPool.h"

class RTMPPublisher {
public:
    RTMPPublisher();
    ~RTMPPublisher();

    int Connect(const char* url, PacketQueue *queue, MemoryPool *pool);
    int Close();

private:
    bool connect(const char* url);
    void run();
    void clean();

private:
    bool quit;
    bool opened;
    std::thread rtmpThread;
    std::mutex threadMutex;

    PacketQueue *queue;
    MemoryPool *pool;
    std::string url;
    RTMP *rtmp;
};

#endif
