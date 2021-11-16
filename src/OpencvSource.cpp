#include "OpencvSource.h"
#include <sys/time.h>
#include <opencv2/imgproc.hpp>

OpencvSource::OpencvSource(int width, int height, const char *name, char a, char b, char c, char d)  {
    mCapture = new cv::VideoCapture();
    if(name == NULL || strcmp(name, "") == 0) {
	    mCapture->open(0);
    }else{
	    int idx = atoi(name);
	    if (idx >= 0 && idx < 10) {
		    mCapture->open(idx);
	    } else {
		    mCapture->open(name);
	    }
    }

	    
    if (!mCapture->isOpened()) {
        std::cout << "can not open " << name << std::endl;
        delete mCapture;
        mCapture = NULL;
    }

    mWidth = width;
    mHeight = height;
}

OpencvSource::~OpencvSource() {
    if (mCapture != NULL) {
        delete(mCapture);
    }
}

char* OpencvSource::getNextFrame() {
    if (mCapture == NULL || !mCapture->isOpened()) {
        std::cout << "device is not open" << std::endl;
        return NULL;
    }

    if(!mCapture->read(mMatSrc))
    {
        std::cout << "get buffer error" << std::endl;
        return NULL;
    }

    cv::cvtColor(mMatSrc, mMatYuv, cv::COLOR_BGR2YUV);
    return (char*)mMatYuv.data;
}

