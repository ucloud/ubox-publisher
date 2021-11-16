#include "PacketQueue.h"

PacketQueue::PacketQueue(int capacity): mCapacity(capacity) {
    mDataBuf = new PacketNode[capacity];
    memset(mDataBuf, 0, capacity * sizeof(PacketNode));
    mHead = mTail = 0;
    last_timestamp = 0;
    last_meta_index = 0;
    last_keyframe_index = 0;
}

PacketQueue::~PacketQueue() {
    delete[] mDataBuf;
}

bool PacketQueue::push(const RTMPPacket& packet, bool metadata, bool keyframe, int64_t timestamp, int picID) {
    std::unique_lock<std::mutex> lock(mMutex);

    if ((mTail + 1) % mCapacity == mHead) {
	    lock.unlock();
	    return false;
    }

    mDataBuf[mTail].packet = packet;
    mDataBuf[mTail].metadata = metadata;
    mDataBuf[mTail].keyframe = keyframe;
    mDataBuf[mTail].timestamp = timestamp;
    mDataBuf[mTail].picID = picID;
    if(timestamp != 0) {
        last_timestamp = timestamp;
        //tlog(TLOG_INFO, "push time:%ld\n", timestamp);
    }
    if(metadata) {
        last_meta_index = mTail;
    }
    if(keyframe) {
        last_keyframe_index = mTail;
    }

    if (++mTail == mCapacity) {
        mTail = 0;
    }
    mEmpty.notify_all();

    lock.unlock();
    return true;
}

PacketNode& PacketQueue::front() {
    std::unique_lock<std::mutex> lock(mMutex);

    while (mHead == mTail) {
        mEmpty.wait(lock);
    }
    mDataBuf[mHead].last_meta = false;
    mDataBuf[mHead].last_keyframe = false;
    if (mHead == last_meta_index)
        mDataBuf[mHead].last_meta = true;
    if (mHead == last_keyframe_index)
        mDataBuf[mHead].last_keyframe = true;
    mDataBuf[mHead].last_timestamp = last_timestamp;

    int nodeIdx = mHead;
    lock.unlock();
    return mDataBuf[nodeIdx];
}

bool PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(mMutex);
    bool metadata = mDataBuf[mHead].metadata;

    if (++mHead == mCapacity) {
        mHead = 0;
    }

    lock.unlock();
    return metadata;
}
