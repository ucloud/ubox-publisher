#include "MediaStream.h"
#include "tinylog/tlog.h"
#include <fcntl.h>
#include <fstream>
#include <linux/videodev2.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef BACK_PNG_FILE
#define BACK_PNG_FILE "/usr/share/ubox-publisher/back.png"
#endif

const char *inputWrhCamera = "wrh-fpga";
const char *inputRTSP = "rtsp";
const char *inputV4L2 = "v4l2";

const char *v4l2FmtYUYV = "YUYV";
const char *v4l2FmtMJPG = "MJPG";

const char *accelNone = "none";
const char *accelJetson = "jetson";
const char *accelIntel = "intel";

const char *codeH264 = "h264";
const char *codeH265 = "h265";

static inline int isNumber(const char s[]) {
  for (int i = 0; s[i] != '\0'; i++) {
    if (isdigit(s[i]) == 0)
      return 0;
  }
  return 1;
}

MediaStream::MediaStream() {
  mOpened = false;
  mEnd = false;
  mFinish = false;
  mInputFPS = 0;
  mAddClock = false;
  mAccel = accelNone;
  mEncode = codeH264;
  mDecode = codeH264;
  mV4l2Fmt = v4l2FmtYUYV;
}

MediaStream::~MediaStream() {}

int MediaStream::setInputType(const char *deviceName) {
  if (isNumber(deviceName))
    mInputType = inputWrhCamera;
  else if (strstr(deviceName, "/dev/video") == deviceName)
    mInputType = inputV4L2;
  else if (strstr(deviceName, "rtsp") == deviceName)
    mInputType = inputRTSP;
  if (mInputType.empty())
    return -1;
  return 0;
}

std::string MediaStream::getAccel() {
  std::ifstream t("/proc/cpuinfo");
  std::stringstream buffer;
  buffer << t.rdbuf();
  if (buffer.str().find("Intel") != std::string::npos)
    return accelIntel;
  else {
    std::ifstream f("/etc/nv_tegra_release");
    if (f.is_open())
      return accelJetson;
  }
  return "";
}

void MediaStream::handleErrorMessage(GstMessage *msg) {
  GError *err = NULL;
  gchar *name, *debug = NULL;

  name = gst_object_get_path_string(msg->src);
  gst_message_parse_error(msg, &err, &debug);

  char buf[256];
  snprintf(buf, sizeof(buf), "ERROR: element %s, %s, debug: %s", name,
           err->message, debug);
  mErrorMsg = buf;

  tlog(TLOG_ERROR, "ERROR: from element %s: %s\n", name, err->message);
  if (debug != NULL)
    tlog(TLOG_ERROR, "Additional debug info:\n%s\n", debug);

  g_clear_error(&err);
  g_free(debug);
  g_free(name);
}

int MediaStream::Open(const char *inputType, const char *deviceName,
                      const char *accel, int srcWidth, int srcHeight,
                      const char *encode, const char *decode, int dstWidth,
                      int dstHeight, int fps, int inputfps, int bitrate,
                      const char *url, bool clockEnable) {
  std::unique_lock<std::mutex> lock(mThreadMutex);
  if (mOpened) {
    tlog(TLOG_INFO, "media stream reopen");
    lock.unlock();
    return -1;
  }

  int ret = setInputType(deviceName);
  if (ret < 0) {
    tlog(TLOG_ERROR, "unknown device, device=%s", deviceName);
    lock.unlock();
    return -1;
  }
  mAccel = getAccel();

  mOpened = true;
  if (clockEnable)
    mAddClock = true;
  if (strlen(inputType) > 0)
    mInputType = inputType;

  mDeviceName = deviceName;
  if (strlen(accel) > 0)
    mAccel = accel;
  mSrcWidth = srcWidth;
  mSrcHeight = srcHeight;
  if (strlen(encode) > 0)
    mEncode = encode;
  if (strlen(decode) > 0)
    mDecode = decode;
  mDstWidth = dstWidth;
  mDstHeight = dstHeight;
  mInputFPS = inputfps;
  if (mInputFPS == 0)
    mInputFPS = fps;
  mFps = fps;
  mBitrate = bitrate;
  mUrl = url;

  tlog(TLOG_INFO, "input=%s,accel=%s,fps=%d,inputfps=%d,encode=%s,decode=%s",
       mInputType.c_str(), mAccel.c_str(), mFps, mInputFPS, mEncode.c_str(),
       mDecode.c_str());

  mQuit = false;
  mRestart = true;
  mStreamThread = std::thread(&MediaStream::loop_run, this);
  lock.unlock();
  return 0;
}

int MediaStream::Close() {
  std::unique_lock<std::mutex> lock(mThreadMutex);
  mQuit = true;
  try {
    mStreamThread.join();
  } catch (...) {
    tlog(TLOG_INFO, "catch stream thread error! stream thread quit!");
    mOpened = false;
    lock.unlock();
    return 0;
  }
  tlog(TLOG_INFO, "stream thread quit!");
  mOpened = false;
  lock.unlock();
  return 0;
}

void MediaStream::SetBitrate(int bitrate) {
  if (mBitrate != bitrate) {
    tlog(TLOG_INFO, "setting bitrate %d Kbps", bitrate);
    GstElement *encoder = gst_bin_get_by_name(GST_BIN(mPipeline), "encoder");
    if (!GST_IS_ELEMENT(encoder)) {
      tlog(TLOG_INFO, "can not find encoder");
      return;
    }
    setBitrate(encoder, bitrate);
  }
  mBitrate = bitrate;
}

int MediaStream::GetStatus(std::string &msg) {
  if (mErrorMsg.empty()) {
    msg = "ok";
    return 0;
  }
  msg = mErrorMsg;
  return 1;
}

void MediaStream::setBitrate(GstElement *encoder, int bitrate) {
  if (mAccel == accelJetson) {
    bitrate *= 1000;
  }
  g_object_set(encoder, "bitrate", bitrate, NULL);
}

int MediaStream::addSource() {
  GstElement *e;
  if (mInputType == inputRTSP) {
    e = gst_element_factory_make("rtspsrc", "src");
    g_object_set(e, "location", mDeviceName.c_str(), "latency", 0, "protocols",
                 4 /*tcp*/, NULL);
  } else if (mInputType == inputWrhCamera) {
    e = gst_element_factory_make("wrhcamerasrc", "src");
    g_object_set(e, "index", atoi(mDeviceName.c_str()), "width", mSrcWidth,
                 "height", mSrcHeight, "fps", mInputFPS, NULL);
  } else {
    // v4l2
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof fmt);

    int v4lfd = open(mDeviceName.c_str(), O_RDONLY);
    if (v4lfd < 0) {
      tlog(TLOG_ERROR, "open %s failed, err=%s(%d)", mDeviceName.c_str(),
           strerror(errno), errno);
      return -1;
    }
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = ioctl(v4lfd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
      tlog(TLOG_ERROR, "ioctl VIDIOC_G_FMT failed, err=%s(%d)", strerror(errno),
           errno);
      return -2;
    }

    if (fmt.fmt.pix.pixelformat == v4l2_fourcc('M', 'J', 'P', 'G')) {
      mV4l2Fmt = v4l2FmtMJPG;
    }

    e = gst_element_factory_make("uv4l2src", "src");
    g_object_set(e, "device", mDeviceName.c_str(), "width", mSrcWidth, "height",
                 mSrcHeight, "fps", mInputFPS, "format", mV4l2Fmt.c_str(), NULL);
    // if (getAccel() != "jetson") { // buggy jetson camera
    //   g_object_set(e, "change", 1, NULL);
    // }
  }
  mElements.push_back(e);
  return 0;
}

void MediaStream::addJpegDec() {
  GstElement *e =
      gst_element_factory_make("jpegdec", "jpegdec");
  mElements.push_back(e);
}

void MediaStream::addClock() {
  GstElement *e =
      gst_element_factory_make("gdkpixbufoverlay", "gdkpixbufoverlay");
  g_object_set(e, "location", BACK_PNG_FILE, "offset-x", 20, "offset-y", 25,
               "overlay-height", 40, "overlay-width", 320, NULL);
  mElements.push_back(e);

  e = gst_element_factory_make("uclockoverlay", "uclockoverlay");
  g_object_set(e, "xpad", 36, "font-desc", "Dejavu Sans Mono", NULL);
  mElements.push_back(e);
}

void MediaStream::addVideoRate() {
  GstElement *e = gst_element_factory_make("videorate", "videorate");
  mElements.push_back(e);
}

void MediaStream::addFilterFramerate() {
  GstElement *e = gst_element_factory_make("capsfilter", "filter_framerate");
  GstCaps *caps;
  caps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION,
                             mFps, 1, NULL);
  g_object_set(e, "caps", caps, NULL);
  gst_caps_unref(caps);
  mElements.push_back(e);
}

void MediaStream::addVideoConvert(const std::string &name) {
  GstElement *e = gst_element_factory_make("videoconvert", name.c_str());
  mElements.push_back(e);
}

void MediaStream::addFilterVideoConvert() {
  if (mAccel == accelNone && mEncode == codeH265) {
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                        "I420", NULL);
    GstElement *e = gst_element_factory_make("capsfilter", "filter_convert");
    g_object_set(e, "caps", caps, NULL);
    gst_caps_unref(caps);
    mElements.push_back(e);
  }
}

void MediaStream::addDepay() {
  GstElement *e;
  if (mDecode == codeH265)
    e = gst_element_factory_make("rtph265depay", "depay");
  else
    e = gst_element_factory_make("rtph264depay", "depay");
  mElements.push_back(e);
}

void MediaStream::addDecoder() {
  GstElement *e;
  if (mDecode == codeH265) {
    if (mAccel == accelIntel) {
      e = gst_element_factory_make("vaapih265dec", "decoder");
    } else {
      e = gst_element_factory_make("avdec_h265", "decoder");
    }
  } else {
    if (mAccel == accelIntel) {
      e = gst_element_factory_make("vaapih264dec", "decoder");
      g_object_set(e, "low-latency", 1, NULL);
    } else {
      e = gst_element_factory_make("avdec_h264", "decoder");
    }
  }
  mElements.push_back(e);
}

void MediaStream::addNVConv() {
  GstElement *e;
  e = gst_element_factory_make("nvvidconv", "conv");
  mElements.push_back(e);
}

void MediaStream::addScale() {
  GstElement *e;
  GstCaps *caps;

  if (mAccel == accelJetson) { // reuse nvvidconv
    caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, mDstWidth,
                               "height", G_TYPE_INT, mDstHeight, NULL);
    GstCapsFeatures *const memory_features{
        gst_caps_features_new("memory:NVMM", nullptr)};
    gst_caps_set_features(caps, 0, memory_features);
    GstElement *f = gst_element_factory_make("capsfilter", "filter_scale");
    g_object_set(f, "caps", caps, NULL);
    mElements.push_back(f);
  } else if (mAccel == accelIntel) {
    e = gst_element_factory_make("vaapipostproc", "scale");
    g_object_set(e, "width", mDstWidth, "height", mDstHeight, "scale-method",
                 2 /*high quality*/, NULL);
    mElements.push_back(e);
  } else {
    e = gst_element_factory_make("videoscale", "scale");
    mElements.push_back(e);

    caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, mDstWidth,
                               "height", G_TYPE_INT, mDstHeight, NULL);
    GstElement *f = gst_element_factory_make("capsfilter", "filter_scale");
    g_object_set(f, "caps", caps, NULL);
    gst_caps_unref(caps);
    mElements.push_back(f);
  }
}

void MediaStream::addEncoder() {
  GstElement *e;
  if (mAccel == accelIntel) {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("vaapih265enc", "encoder");
      g_object_set(e, "tune", 1 /*high compression*/, "quality-level", 2,
                   "cpb-length", 800, "rate-control", 2 /*cbr*/, NULL);
    } else {
      e = gst_element_factory_make("vaapih264enc", "encoder");
    }
    g_object_set(e, "tune", 1 /*high compression*/, "quality-level", 2,
                 "cpb-length", 800, "rate-control", 2 /*cbr*/, "cabac", TRUE,
                 NULL);
    setBitrate(e, mBitrate);
  } else if (mAccel == accelJetson) {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("nvv4l2h265enc", "encoder");
      g_object_set(e, "profile", 0 /*Main*/, "preset-level", 3 /*normal*/,
                   "maxperf-enable", TRUE, "vbv-size", 1000000, "control-rate",
                   1 /*constant_bitrate*/, "bitrate", mBitrate * 1000,
                   "peak-bitrate", mBitrate * 1000, "num-B-Frames", 0, NULL);
    } else {
      e = gst_element_factory_make("nvv4l2h264enc", "encoder");
      g_object_set(e, "profile", 4 /*High*/, "preset-level", 3 /*normal*/,
                   "maxperf-enable", TRUE, "vbv-size", 1000000, "control-rate",
                   1 /*constant_bitrate*/, "bitrate", mBitrate * 1000,
                   "peak-bitrate", mBitrate * 1000, "num-B-Frames", 0,
                   "cabac-entropy-coding", TRUE, NULL);
    }
  } else {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("x265enc", "encoder");
      g_object_set(e, "speed-preset", 1 /*ultrafast*/, "tune",
                   4 /*zerolatency*/, "option-string", "keyint=10", NULL);
    } else {
      e = gst_element_factory_make("x264enc", "encoder");
      g_object_set(e, "speed-preset", 1 /*ultrafast*/, "tune",
                   4 /*zerolatency*/, "key-int-max", 10, "vbv-buf-capacity",
                   600, NULL);
    }
    setBitrate(e, mBitrate);
  }
  mElements.push_back(e);
}

void MediaStream::addParser() {
  GstElement *e;
  if (mEncode == codeH265)
    e = gst_element_factory_make("h265parse", "parser");
  else
    e = gst_element_factory_make("h264parse", "parser");
  mElements.push_back(e);
}

void MediaStream::addFilterProfile() {
  GstElement *e = gst_element_factory_make("capsfilter", "filter_profile");
  std::string profile = "high";
  GstCaps *caps = gst_caps_new_simple("video/x-h264", "profile", G_TYPE_STRING,
                                      profile.c_str(), NULL);

  g_object_set(e, "caps", caps, NULL);
  gst_caps_unref(caps);
  mElements.push_back(e);
}

void MediaStream::addFlvmux() {
  GstElement *e = gst_element_factory_make("uflvmux", "flvmux");
  g_object_set(e, "streamable", true, NULL);
  mElements.push_back(e);
}

void MediaStream::addRTMPSink() {
  GstElement *e = gst_element_factory_make("rtmpsink", "sink");
  std::string s = mUrl + " live=1 timeout=3 wtimeout=5";
  g_object_set(e, "location", s.c_str(), NULL);
  mElements.push_back(e);
}

int MediaStream::setupPipeline() {
  mPipeline = gst_pipeline_new("publisher");
  if (addSource() < 0) {
    tlog(TLOG_ERROR, "add element src failed");
    return -1;
  }
  if (mInputType == inputV4L2 && mV4l2Fmt == v4l2FmtMJPG) {
      addJpegDec();
  }
  if (mInputType == inputRTSP) {
    addDepay();
    addDecoder();
    if (mFps > 0) {
      addVideoRate();
      addFilterFramerate();
    }
  } else if (mInputType == inputV4L2 || mInputType == inputWrhCamera) {
    if (mInputType == inputWrhCamera)
      addVideoConvert("videoconvert-wrh");

    if (mAddClock)
      addClock();
    if (mFps > 0 && mFps != mInputFPS) {
      addVideoRate();
      addFilterFramerate();
    }
  }

  if (mAccel == accelJetson)
    addNVConv();
  // scale
  if (mSrcWidth > mDstWidth && mSrcHeight > mDstHeight) {
    addScale();
  }

  addVideoConvert("videoconvert");
  addFilterVideoConvert();

  addEncoder();
  if (mEncode == codeH264 && mAccel == accelIntel)
    addFilterProfile();
  addParser();

  addFlvmux();
  addRTMPSink();

  for (auto e : mElements) {
    gst_bin_add(GST_BIN(mPipeline), e);
  }

  auto it = mElements.begin();
  if (mInputType == inputRTSP) {
    it++;
  }
  for (; it != mElements.end() - 1; it++) {
    gboolean ok = gst_element_link(*it, *(it + 1));
    if (!ok) {
      gchar *src = gst_element_get_name(*it);
      gchar *dst = gst_element_get_name(*(it + 1));
      char buf[128];
      snprintf(buf, sizeof(buf), "gst link failed, src: %s, dst: %s", src, dst);
      mErrorMsg = buf;
      tlog(TLOG_ERROR, "%s", mErrorMsg.c_str());
      g_free(src);
      g_free(dst);
      gst_object_unref(mPipeline);
      return -1;
    }
  }

  if (mInputType == inputRTSP) {
    g_signal_connect(mElements[0], "pad-added", G_CALLBACK(&onRtspPadAdded),
                     this);
  }
  gst_element_add_property_deep_notify_watch(mPipeline, NULL, TRUE);

  GstStateChangeReturn res =
      gst_element_set_state(mPipeline, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    GstMessage *err_msg;
    GstBus *bus = gst_element_get_bus(mPipeline);
    if ((err_msg = gst_bus_poll(bus, GST_MESSAGE_ERROR, 0))) {
      tlog(TLOG_ERROR, "play failed");
      handleErrorMessage(err_msg);
      gst_message_unref(err_msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(mPipeline, GST_STATE_NULL);
    gst_object_unref(mPipeline);
    return 1;
  }
  return 0;
}

void MediaStream::loop_run() {
  int i = 0;
  pthread_t h;
  while (!mQuit && mRestart) {
    tlog(TLOG_INFO, "run pipeline");
    i = 0;
    mFinish = mEnd = false;
    std::thread th = std::thread(&MediaStream::run, this);
    h = th.native_handle();
    th.detach();
    while (!mEnd) {
      if (mFinish)
        i++;
      if (i > 25) {
        // th is deadlock
        tlog(TLOG_WARN, "deadlock detected");
        pthread_cancel(h);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }
  tlog(TLOG_INFO, "media stream quit");
}

void MediaStream::run() {
  gst_init(NULL, NULL);
  mRestart = false;
  int result = setupPipeline();
  if (result > 0) {
    mElements.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    mRestart = true;
    mEnd = true;
    return;
  } else if (result < 0) {
    mElements.clear();
    mEnd = true;
    return;
  }

  GstBus *bus = gst_element_get_bus(mPipeline);
  if (bus == NULL) {
    tlog(TLOG_WARN, "get bus failed");
    gst_element_set_state(mPipeline, GST_STATE_NULL);
    gst_object_unref(mPipeline);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    mElements.clear();
    mRestart = true;
    mEnd = true;
    return;
  }

  do {
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, 200 * GST_MSECOND,
        (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR |
                         GST_MESSAGE_EOS));

    if (msg == NULL) {
      mErrorMsg = "";
      continue;
    }

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      handleErrorMessage(msg);
      mRestart = true;
      break;
    case GST_MESSAGE_EOS:
      tlog(TLOG_ERROR, "End-Of-Stream reached.");
      mRestart = true;
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(mPipeline)) {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state,
                                        &pending_state);
        tlog(TLOG_INFO, "Pipeline state changed from %s to %s:",
             gst_element_state_get_name(old_state),
             gst_element_state_get_name(new_state));
        if (old_state == GST_STATE_PLAYING &&
            (new_state == GST_STATE_PAUSED || new_state == GST_STATE_NULL)) {
          GstMessage *err_msg;
          GstBus *bus = gst_element_get_bus(mPipeline);
          if ((err_msg = gst_bus_poll(bus, GST_MESSAGE_ERROR, 0))) {
            tlog(TLOG_ERROR, "play failed");
            handleErrorMessage(err_msg);
            gst_message_unref(err_msg);
          }
          gst_object_unref(bus);
          mRestart = true;
        }
      }
      break;
    default:
      /* We should not reach here */
      tlog(TLOG_WARN, "Unexpected message received.");
      break;
    }
    gst_message_unref(msg);
  } while (!mRestart && !mQuit);

  mFinish = true;
  gst_object_unref(bus);
  mElements.clear();
  gst_element_set_state(mPipeline, GST_STATE_PAUSED);
  gst_element_set_state(mPipeline, GST_STATE_NULL);
  gst_object_unref(mPipeline);
  tlog(TLOG_INFO, "run end and quit normally");
  mEnd = true;
}

void MediaStream::onRtspPadAdded(GstElement *src, GstPad *srcPad,
                                 gpointer userData) {
  MediaStream *self = (MediaStream *)userData;
  GstElement *depay = gst_bin_get_by_name(GST_BIN(self->mPipeline), "depay");
  if (depay == NULL) {
    tlog(TLOG_ERROR, "no depay");
    return;
  }
  GstPad *sinkPad = gst_element_get_static_pad(depay, "sink");
  GstCaps *newPadCaps = NULL;
  GstStructure *newPadStruct = NULL;
  const gchar *newPadType = NULL;

  if (gst_pad_is_linked(sinkPad)) {
    tlog(TLOG_INFO, "linked already");
    gst_object_unref(sinkPad);
    return;
  }

  newPadCaps = gst_pad_get_current_caps(srcPad);
  newPadStruct = gst_caps_get_structure(newPadCaps, 0);
  newPadType = gst_structure_get_name(newPadStruct);

  if (!g_str_has_prefix(newPadType, "application/x-rtp")) {
    tlog(TLOG_INFO, "got %s, not rtsp", newPadType);
  } else {
    const char *name = gst_structure_get_string(newPadStruct, "encoding-name");
    tlog(TLOG_DEBUG, "new pad added, type=%s, media=%s, encoding:%s",
         newPadType, gst_structure_get_string(newPadStruct, "media"), name);
    if (g_str_has_prefix(name, "H264") || g_str_has_prefix(name, "H265")) {
      gst_pad_link(srcPad, sinkPad);
    } else {
      tlog(TLOG_WARN, "unsupported encode %s", name);
    }
  }

  gst_caps_unref(newPadCaps);
  gst_object_unref(sinkPad);
}
