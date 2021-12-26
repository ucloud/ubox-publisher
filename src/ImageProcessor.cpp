#include "ImageProcessor.h"
#include <sys/time.h>

ImageProcessor::ImageProcessor() {

}

ImageProcessor::~ImageProcessor() {

}

const unsigned char* ImageProcessor::YUY2I420(const unsigned char *data, int srcWidth, int srcHeight, int dstWidth, int dstHeight) {
    cv::Size srcSize(srcWidth, srcHeight);
    matYUV = cv::Mat(srcSize, CV_8UC2, (void*)data);
    cv::cvtColor(matYUV, mat, cv::COLOR_YUV2BGR_YVYU );
    resize(mat, matResize, dstWidth, dstHeight);
    //addTime(matResize);
    cv::cvtColor(matResize, matDst, cv::COLOR_BGR2YUV_YV12);
    return (const unsigned char*)matDst.data;
}


void ImageProcessor::addTime(cv::Mat& mat) {
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    struct tm nowTime;
    localtime_r(&curTime.tv_sec, &nowTime);
    char buffer[32];
    char timeChar[64];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &nowTime);
    snprintf(timeChar, sizeof(timeChar), "%s.%03ld", buffer, curTime.tv_usec/1000);

    cv::Point origin(10, 30);
    cv::putText(mat, timeChar, origin, cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 0, 0), 2, 8, false);
}

void ImageProcessor::resize(cv::Mat &srcMat, cv::Mat &dstMat, int dstWidth, int dstHeight) {
    cv::Size srcSize = srcMat.size();
    float src_aspect_ratio = float(srcSize.width)/float(srcSize.height);
    float dst_aspect_ratio = float(dstWidth)/float(dstHeight);
    cv::Mat mat_tmp_resize;
    if (src_aspect_ratio > dst_aspect_ratio) {
        int _width = srcSize.height*dst_aspect_ratio;
        int _x = (srcSize.width - _width)/2;
        cv::Rect rect(_x, 0, _width, srcSize.height);
        mat_tmp_resize = srcMat(rect);
    } else {
        int _height = srcSize.width/dst_aspect_ratio;
        int _y = (srcSize.height - _height)/2;
        cv::Rect rect(0, _y, srcSize.width, _height);
        mat_tmp_resize = srcMat(rect);
    }
    cv::Size outSize(dstWidth, dstHeight);
    cv::resize(mat_tmp_resize, dstMat, outSize, 0, 0, cv::INTER_AREA);
}
