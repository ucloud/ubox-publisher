#include "AACStream.h"
#include <iostream>
#include "tinylog/tlog.h"

AACStream::AACStream(AudioSource& source, PacketQueue& queue, MemoryPool& pool, int bitrate): 
    mSource(source), mQueue(queue), mPool(pool),
    mEncoder(source.getSampleRate(), source.getChannals(), bitrate),mExit(false) {}

void AACStream::run() {
    tlog(TLOG_INFO, "AAC stream is running\n");
    if (!mSource.isOpened()) {
        tlog(TLOG_INFO, "audio device is not open\n");
        tlog(TLOG_INFO, "AAC stream is stopped\n");
        return;
    }

    mSource.setMaxSample(mEncoder.getMaxSample());

    char *buf;
    RTMPPacket packet;
    AACRTMPPackager packager;
    std::pair<int, char*> frame;
    std::pair<int, char*> result = mEncoder.getMetadata();

    buf = mPool.getChunk(256);
    packet = packager.metadata(buf, result.second, result.first);
    mQueue.push(packet);

    while (!mExit) {
        frame = mSource.getNextFrames();
        if(frame.second == NULL) {
            tlog(TLOG_INFO, "audio device get null frame\n");
            break;
        }
        result = mEncoder.encode(frame.first, frame.second);

        if (result.first != 0) {
            buf = mPool.getChunk(packager.getBodyLength(result.first));
            packet = packager.pack(buf, result.second, result.first);
            mQueue.push(packet);
        }
    }
    tlog(TLOG_INFO, "AAC stream is stopped\n");
}

void AACStream::quit() {
    mExit = true;
}