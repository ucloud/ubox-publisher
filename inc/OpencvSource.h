#ifndef _OPENCV_SOURCE_H_
#define _OPENCV_SOURCE_H_

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <opencv2/videoio.hpp>
#include <cstring>
#include <iostream>
#include "VideoSource.h"

class OpencvSource: public VideoSource {
public:
    OpencvSource(int width, int height, const char* name,
               char a = 'Y', char b = 'U', char c = 'Y', char d = 'V');

    virtual ~OpencvSource();

    virtual int getWidth() const { return mWidth; }

    virtual int getHeight() const { return mHeight; }

    virtual bool isOpened() const {
        if (mCapture == NULL) {
		std::cout << "mcapture is null";
		return false;
	}
        else return mCapture->isOpened();
    }

    virtual char* getNextFrame();
private:
    int mWidth;
    int mHeight;
    cv::VideoCapture *mCapture;
    cv::Mat mMatSrc, mMatYuv;
};

#endif
