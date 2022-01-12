#ifndef _MEDIASTREAM_H_20210909_
#define _MEDIASTREAM_H_20210909_

#include <unistd.h>
#include <thread>
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
            bool copy, int dstWidth, int dstHeight, int fps, int bitrate, const char *url);
    int Close();

    void SetBitrate(int bitrate);

private:
    void setBitrate(GstElement *encoder, int bitrate);
    static void onRtspPadAdded(GstElement *src, GstPad *src_pad, gpointer user_data);
    int setupPipeline();
    void addSource();
    void addVideoRate();
    void addFilterFramerate();
    void addDepay();
    void addDecoder();
    void addVideoConvert();
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

private:
    bool mQuit;
    bool mOpened;
    bool mRestart;
    std::thread mStreamThread;
    std::mutex mThreadMutex;

    std::string mInputType;
    std::string mDeviceName;
    std::string mAccel;
    std::string mUrl;
    int mSrcWidth;
    int mSrcHeight;
    bool mStreamCopy;
    int mFps;
    int mDstWidth;
    int mDstHeight;
    int mBitrate;
    std::vector<GstElement *> mElements;
    GstElement* mPipeline;
    GstElement* mDepay;
};

#endif//_MEDIASTREAM_H_20210909_
