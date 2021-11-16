#include <chrono>
#include <iostream>
#include <opencv2/videoio.hpp>

int main(int argc, char** argv) {
    std::string device = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);

    cv::Mat mat;
    cv::VideoCapture capture;
    capture.open(device.c_str(), cv::CAP_V4L2);
    capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    if (!capture.isOpened()) {
        std::cout << "can not open " << device << std::endl;
        return 0;
    }

    int index = 0;
    double tm_avg = 0;
    while(index < 5000) {
        auto start = std::chrono::system_clock::now();
        capture.read(mat);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> tm = end - start;
        tm_avg = (tm_avg*index + tm.count())/++index;
        printf("last %f, avg %f\n", tm.count(), tm_avg);
    }
    return 0;
}

