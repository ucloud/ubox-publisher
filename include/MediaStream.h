#ifndef _MEDIASTREAM_H_20210909_
#define _MEDIASTREAM_H_20210909_

#include <unistd.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
extern "C" {
#include <gst/gst.h>
#include <gst/gstbus.h>
}

class MediaStream {
public:
    MediaStream();
    ~MediaStream();

    int Open(const char *inputType, const char* deviceName, const char* accel, int srcWidth, int srcHeight,
            const char *encode, const char *decode, int dstWidth, int dstHeight, int fps, int inputfps, int bitrate, const char *url, bool clockEnable);
    int Close();
    void SetBitrate(int bitrate);
    int GetStatus(std::string &msg);

private:
    void handleErrorMessage(GstMessage *msg);
    void setBitrate(GstElement *encoder, int bitrate);
    static void onRtspPadAdded(GstElement *src, GstPad *src_pad, gpointer user_data);
    int setupPipeline();
    int addSource();
    void addJpegDec();
    void addClock();
    void addVideoRate();
    void addFilterFramerate();
    void addDepay();
    void addDecoder();
    void addVideoConvert(const std::string &name);
    void addFilterVideoConvert();
    void addNVConv();
    void addScale();
    void addFilterScale();
    void addEncoder();
    void addParser();
    void addFlvmux();
    void addFilterProfile();
    void addRTMPSink();
    void loop_run();
    void run();
    void process();
    int setInputType(const char *deviceName);
    std::string getAccel();

private:
    bool mQuit;
    bool mOpened;
    bool mRestart;
    bool mEnd;
    bool mFinish;
    std::thread mStreamThread;
    std::mutex mThreadMutex;

    std::string mInputType;
    std::string mDeviceName;
    std::string mAccel;
    std::string mUrl;
    int mSrcWidth;
    int mSrcHeight;
    std::string mEncode;
    std::string mDecode;
    bool mAddClock;
    int mFps;
    int mInputFPS;
    int mDstWidth;
    int mDstHeight;
    int mBitrate;
    std::vector<GstElement *> mElements;
    GstElement* mPipeline;
    GstElement* mDepay;
    std::string mErrorMsg;
    std::string mV4l2Fmt;
};

#endif//_MEDIASTREAM_H_20210909_
