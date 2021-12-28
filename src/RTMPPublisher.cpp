#include "RTMPPublisher.h"
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include "tinylog/tlog.h"

#define RTMP_TIMEOUT 5

RTMPPublisher::RTMPPublisher(){
    opened = false;
    rtmp = NULL;
}

RTMPPublisher::~RTMPPublisher() {
    if (rtmp) {
        if (RTMP_IsConnected(rtmp)) {
            RTMP_Close(rtmp);
        }
        RTMP_Free(rtmp);
        rtmp = NULL;
    }
}

int RTMPPublisher::Connect(const char *url, PacketQueue *queue, MemoryPool *pool) {
    std::unique_lock<std::mutex> lock(threadMutex);
    if (opened) {
        tlog(TLOG_INFO, "rtmp reconnect\n");
        lock.unlock();
        return -1;
    }
    opened = true;
    lock.unlock();

    this->url = url;
    this->queue = queue;
    this->pool = pool;

    if (rtmp) {
        if (RTMP_IsConnected(rtmp)) {
            RTMP_Close(rtmp);
        }
        RTMP_Free(rtmp);
        rtmp = NULL;
    }

    rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    rtmp->Link.timeout = RTMP_TIMEOUT;

    if (!connect(url)) {
        tlog(TLOG_INFO, "rtmp connect failed\n");
        RTMP_Free(rtmp);
        rtmp = NULL;
        opened = false;
        return -1;
    }

    quit = false;
    rtmpThread = std::thread(&RTMPPublisher::run, this);
    return 0;
}

bool RTMPPublisher::connect(const char *url) {
    if (!RTMP_SetupURL(rtmp, (char*)url)) {
        return false;
    }

    RTMP_EnableWrite(rtmp);

    if (!RTMP_Connect(rtmp, NULL)) {
        return false;
    }
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDTIMEO, ( char *)&timeout, sizeof (timeout)) == -1) {
        tlog(TLOG_INFO, "rtmp set send timeout failed\n");
    }

    if (!RTMP_ConnectStream(rtmp, 0)) {
        RTMP_Close(rtmp);
        return false;
    }

    return true;
}


void RTMPPublisher::run() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    while (!quit) {
        if (!RTMP_IsConnected(rtmp)) {
            tlog(TLOG_INFO, "rtmp server is not connected: %s\n", url.c_str());
            sleep(1);
            connect(url.c_str());
            tlog(TLOG_INFO, "rtmp reconnect server %s\n", url.c_str());
            continue;
        }

        PacketNode &node = queue->front();
        if( node.timestamp != 0 &&
            (node.last_timestamp - node.timestamp > 500) &&
            (!node.last_meta) &&
            (!node.last_keyframe)
        ) {
            tlog(TLOG_INFO, "rtmp packet timestamp %ld, last timestamp %ld,  delayed, dropped! meta %d, keyframe %d, last_meta %d, last_keyframe %d\n",
                   node.timestamp, node.last_timestamp, node.metadata, node.keyframe, node.last_meta, node.last_keyframe);

            int bytes = node.packet.m_nBodySize + RTMP_MAX_HEADER_SIZE;
            char *body = node.packet.m_body - RTMP_MAX_HEADER_SIZE;

            if (!queue->pop()) {
                pool->putChunk(bytes, body);
            }
            continue;
        }
        node.packet.m_nInfoField2 = rtmp->m_stream_id;
        node.packet.m_nTimeStamp = node.picID;

        //std::cout << "RTMP_SendPacket" <<  packet.m_nTimeStamp << std::endl;
        //auto send_start = std::chrono::system_clock::now();
        if (!RTMP_SendPacket(rtmp, &node.packet, 1)) {
            tlog(TLOG_INFO, "rtmp send packet error!\n");
            sleep(1);
            continue;
        }
        auto send_end = std::chrono::system_clock::now();
        //tlog(TLOG_INFO, "frame id: %d, rtmp send start time %ld, rtmp send end time %ld\n", node.picID,
        tlog(TLOG_INFO, "frame id: %d, rtmp send end time %ld\n", node.picID,
             //std::chrono::duration_cast<std::chrono::milliseconds>(send_start.time_since_epoch()).count(),
             std::chrono::duration_cast<std::chrono::milliseconds>(send_end.time_since_epoch()).count());


        int bytes = node.packet.m_nBodySize + RTMP_MAX_HEADER_SIZE;
        char *body = node.packet.m_body - RTMP_MAX_HEADER_SIZE;

        if (!queue->pop()) {
            pool->putChunk(bytes, body);
        }
    }
}

int RTMPPublisher::Close() {
    std::unique_lock<std::mutex> lock(threadMutex);
    quit = true;
    try {
        rtmpThread.join();
    } catch (...){
        tlog(TLOG_INFO, "catch rtmp thread error\n");
        tlog(TLOG_INFO, "rtmp thread quit!\n");
        clean();
        lock.unlock();
        return 0;
    }
    tlog(TLOG_INFO, "rtmp thread quit!\n");
    clean();
    lock.unlock();
    return 0;
}

void RTMPPublisher::clean() {
    opened = false;
    if (rtmp) {
        if (RTMP_IsConnected(rtmp)) {
            RTMP_Close(rtmp);
        }
        RTMP_Free(rtmp);
        rtmp = NULL;
    }
}
