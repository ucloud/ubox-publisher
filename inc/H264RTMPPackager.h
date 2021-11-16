#ifndef _H264_RTMP_Packager_H_
#define _H264_RTMP_Packager_H_

#include <cstring>
#include "RTMPPackager.h"

class H264RTMPPackager: public RTMPPackager {
public:
    virtual RTMPPacket pack(char* buf, const char* data, int length) const;

    virtual RTMPPacket metadata(char* buf, const char* data, int length) const;

    virtual int getBodyLength(int length) const { return length + 5 + RTMP_MAX_HEADER_SIZE; }

    static bool isKeyFrame(char* data) { unsigned char ch =  data[4] & 0x1f; return (ch == 5);}
};

#endif
