#include "MediaStream.h"
#include "tinylog/tlog.h"
#include <fstream>
#include <sstream>

#ifndef BACK_PNG_FILE
#define BACK_PNG_FILE "/usr/share/ubox-publisher/back.png"
#endif

const char *inputWrhCamera = "wrh-fpga";
const char *inputRTSP = "rtsp";
const char *inputV4L2 = "v4l2";

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
  mFps = fps;
  mInputFPS = inputfps;
  if (mInputFPS == 0)
    mInputFPS = 60;
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

void MediaStream::addSource() {
  GstElement *e;
  if (mInputType == inputRTSP) {
    e = gst_element_factory_make("rtspsrc", "src");
    g_object_set(e, "location", mDeviceName.c_str(), "latency", 0, "protocols",
                 4 /*tcp*/, NULL);
  } else if (mInputType == inputWrhCamera) {
    e = gst_element_factory_make("wrhcamerasrc", "src");
    g_object_set(e, "index", atoi(mDeviceName.c_str()), "width", mSrcWidth,
                 "height", mSrcHeight, "fps", 20, NULL);
  } else {
    e = gst_element_factory_make("uv4l2src", "src");
    g_object_set(e, "device", mDeviceName.c_str(), "width", mSrcWidth, "height",
                 mSrcHeight, "fps", mInputFPS, NULL);
    // if (getAccel() != "jetson") { // buggy jetson camera
    //   g_object_set(e, "change", 1, NULL);
    // }
  }
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
  if (mAccel == accelJetson) // reuse nvvidconv
    return;
  else if (mAccel == accelIntel && mInputType != inputWrhCamera) {
    e = gst_element_factory_make("vaapipostproc", "scale");
    g_object_set(e, "width", mDstWidth, "height", mDstHeight, "scale-method",
                 2 /*high quality*/, NULL);
  } else
    e = gst_element_factory_make("videoscale", "scale");
  mElements.push_back(e);
}

void MediaStream::addFilterScale() {
  GstElement *e = gst_element_factory_make("capsfilter", "filter_scale");
  GstCaps *caps;
  if (mAccel == accelJetson) {
    caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, mDstWidth,
                               "height", G_TYPE_INT, mDstHeight, NULL);
    GstCapsFeatures *const memory_features{
        gst_caps_features_new("memory:NVMM", nullptr)};
    gst_caps_set_features(caps, 0, memory_features);
  } else {
    caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, mDstWidth,
                               "height", G_TYPE_INT, mDstHeight, NULL);
  }
  g_object_set(e, "caps", caps, NULL);
  gst_caps_unref(caps);
  mElements.push_back(e);
}

void MediaStream::addEncoder() {
  GstElement *e;
  if (mAccel == accelIntel) {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("vaapih265enc", "encoder");
    } else {
      e = gst_element_factory_make("vaapih264enc", "encoder");
    }
    g_object_set(e, "quality-level", 3, "cpb-length", 800, "rate-control",
                 2 /*cbr*/, "keyframe-period", 10, NULL);
    setBitrate(e, mBitrate);
  } else if (mAccel == accelJetson) {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("nvv4l2h265enc", "encoder");
      g_object_set(e, "vbv-size", 1000000, "profile", 0 /*Main*/,
                   "iframeinterval", 10, "control-rate", 1 /*constant_bitrate*/,
                   "maxperf-enable", 1, "bitrate", mBitrate * 1000, NULL);
    } else {
      e = gst_element_factory_make("nvv4l2h264enc", "encoder");
      g_object_set(e, "vbv-size", 1000000, "profile", 4 /*High*/,
                   "iframeinterval", 10, "control-rate", 1 /*constant_bitrate*/,
                   "maxperf-enable", 1, "bitrate", mBitrate * 1000, NULL);
    }
  } else {
    if (mEncode == codeH265) {
      e = gst_element_factory_make("x265enc", "encoder");
      g_object_set(e, "tune", 4 /*zerolatency*/, "speed-preset",
                   1 /*ultrafast*/, NULL);
    } else {
      e = gst_element_factory_make("x264enc", "encoder");
      g_object_set(e, "key-int-max", 10, "tune", 4 /*zerolatency*/,
                   "speed-preset", 1 /*ultrafast*/, "vbv-buf-capacity", 300,
                   NULL);
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
  addSource();
  if (mInputType == inputRTSP) {
    addDepay();
    addDecoder();
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
  if (mSrcWidth != mDstWidth && mSrcHeight != mDstHeight) {
    addScale();
    if (mAccel != accelIntel && mInputType != inputWrhCamera)
      addFilterScale();
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
        if (new_state == GST_STATE_PAUSED || new_state == GST_STATE_NULL) {
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
