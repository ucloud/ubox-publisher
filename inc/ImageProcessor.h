#ifndef _IMAGE_PROCESSOR_H_20210909_
#define _IMAGE_PROCESSOR_H_20210909_

#include <opencv2/imgproc.hpp>

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    const unsigned char* YUY2I420(const unsigned char* data, int srcWidth, int srcHeight, int dstWidth, int dstHeight);

private:
    void addTime(cv::Mat& mat);
    void resize(cv::Mat& srcMat, cv::Mat& dstMat, int dstWidth, int dstHeight);

private:
    cv::Mat matYUV, mat, matResize, matDst;
};

#endif//_IMAGE_PROCESSOR_H_20210909_
