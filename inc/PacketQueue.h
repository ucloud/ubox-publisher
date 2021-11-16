#ifndef _PACKET_QUEUE_H_
#define _PACKET_QUEUE_H_

#include <cstring>
#include <mutex>
#include <condition_variable>
#include <librtmp/rtmp.h>
#include "RTMPPackager.h"

// single consumer !!!

struct PacketNode {
    RTMPPacket packet;
    bool metadata;
    bool keyframe;
    int64_t timestamp;
    bool last_meta;
    bool last_keyframe;
    int64_t last_timestamp;
    int picID;
};

class PacketQueue {
public:
    PacketQueue(int capacity = 100);

    ~PacketQueue();

    bool push(const RTMPPacket& packet, bool metadata = false, bool keyframe = false, int64_t timestamp = 0, int picID = 0);
    PacketNode& front();
    bool pop();

private:
    PacketNode *mDataBuf;
    std::mutex mMutex;
    std::condition_variable mFull;
    std::condition_variable mEmpty;
    int mHead;
    int mTail;
    int mCapacity;
    int64_t last_timestamp;
    int last_meta_index;
    int last_keyframe_index;
};

#endif
