#include "MediaStream.h"
#include "tinylog/tlog.h"

const char *inputWrhCamera = "wrh-fpga";
const char *inputRTSP = "rtsp";
const char *inputV4L2 = "v4l2";

const char *accelJetson = "jetson";
const char *accelIntel = "intel";

MediaStream::MediaStream() { mOpened = false; }

MediaStream::~MediaStream() {}

int MediaStream::Open(const char *inputType, const char *deviceName,
                      const char *accel, int srcWidth, int srcHeight, bool copy,
                      int dstWidth, int dstHeight, int fps, int bitrate,
                      const char *url) {
  std::unique_lock<std::mutex> lock(mThreadMutex);
  if (mOpened) {
    tlog(TLOG_INFO, "media stream reopen");
    lock.unlock();
    return -1;
  }
  mOpened = true;
  lock.unlock();

  if (inputType == NULL || strlen(inputType) == 0)
    mInputType = inputV4L2;
  else
    mInputType = inputType;
  mDeviceName = deviceName;
  mAccel = accel;
  mSrcWidth = srcWidth;
  mSrcHeight = srcHeight;
  mStreamCopy = copy;
  mDstWidth = dstWidth;
  mDstHeight = dstHeight;
  mFps = fps;
  mBitrate = bitrate;
  mUrl = url;

  mQuit = false;
  mRestart = true;
  mStreamThread = std::thread(&MediaStream::loop_run, this);
  return 0;
}

int MediaStream::Close() {
  std::unique_lock<std::mutex> lock(mThreadMutex);
  mQuit = true;
  gst_element_set_state(mPipeline, GST_STATE_NULL);
  try {
    mStreamThread.join();
  } catch (...) {
    tlog(TLOG_INFO, "catch stream thread error!");
    tlog(TLOG_INFO, "stream thread quit!");
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
  if (mStreamCopy) {
    tlog(TLOG_INFO, "stream copy, can not set bitrate");
    return;
  }
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
    g_object_set(e, "index", atoi(mDeviceName.c_str()), "width", mSrcWidth, "height",
                 mSrcHeight, NULL);
  } else {
    e = gst_element_factory_make("uv4l2src", "src");
    g_object_set(e, "device", mDeviceName.c_str(), "width", mSrcWidth, "height", mSrcHeight, NULL);
  }
  mElements.push_back(e);
}

void MediaStream::addVideoRate() {
  GstElement *e = gst_element_factory_make("videorate", "videorate");
  mElements.push_back(e);
}

void MediaStream::addFilterFramerate() {
  GstElement *e = gst_element_factory_make("capsfilter", "filter_framerate");
  GstCaps *caps;
  caps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, mFps, 1,
                               NULL);
  g_object_set(e, "caps", caps, NULL);
  gst_caps_unref(caps);
  mElements.push_back(e);
}

void MediaStream::addVideoConvert() {
  GstElement *e = gst_element_factory_make("videoconvert", "videoconvert");
  mElements.push_back(e);
}

void MediaStream::addDepay() {
  GstElement *e = gst_element_factory_make("rtph264depay", "depay");
  mElements.push_back(e);
}

void MediaStream::addDecoder() {
  GstElement *e;
  if (mAccel == accelIntel) {
    e = gst_element_factory_make("vaapih264dec", "decoder");
    g_object_set(e, "low-latency", 1, NULL);
  } else {
    e = gst_element_factory_make("avdec_h264", "decoder");
  }
  mElements.push_back(e);
}

void MediaStream::addScale() {
  GstElement *e;
  if (mAccel == accelJetson)
    e = gst_element_factory_make("nvvidconv", "scale");
  else
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
    e = gst_element_factory_make("vaapih264enc", "encoder");
    g_object_set(e, "quality-level", 1, "cpb-length", 500, "rate-control", 4,
                 "keyframe-period", 10,
                 NULL); // constant
    setBitrate(e, mBitrate);
  } else if (mAccel == accelJetson) {
    e = gst_element_factory_make("nvv4l2h264enc", "encoder");
    g_object_set(e, "profile", 4/*high*/, "iframeinterval", 10, "control-rate", 1, "maxperf-enable",
                 1, "bitrate", mBitrate * 1000, NULL);
  } else {
    e = gst_element_factory_make("x264enc", "encoder");
    g_object_set(e, "tune", 4, "speed-preset", 1, "vbv-buf-capacity", 100,
                 NULL); // lowlatency,ultrafast
    setBitrate(e, mBitrate);
  }
  mElements.push_back(e);
}

void MediaStream::addParser() {
  GstElement *e;
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
  GstElement *e = gst_element_factory_make("flvmux", "flvmux");
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
  } else if (mInputType == inputV4L2) {
    addVideoRate();
    addFilterFramerate();
  }
  // scale
  addScale();
  addFilterScale();

  if (mInputType == inputWrhCamera)
    addVideoConvert();

  addEncoder();
  if (mAccel == accelJetson)
    addParser();
  else
    addFilterProfile();

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
      tlog(TLOG_ERROR, "gst link failed, src: %s, dst: %s", src, dst);
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
    tlog(TLOG_ERROR, "play failed");
    gst_object_unref(mPipeline);
    return -2;
  }
  return 0;
}

void MediaStream::loop_run() {
  while (!mQuit && mRestart) {
    tlog(TLOG_INFO, "run pipeline");
    std::thread th = std::thread(&MediaStream::run, this);
    try {
      th.join();
    } catch (...) {
      tlog(TLOG_INFO, "pipeline quit");
    }
  }
  tlog(TLOG_INFO, "stream thread quit");
}

void MediaStream::run() {
  gst_init(NULL, NULL);
  mRestart = false;
  int result = setupPipeline();
  if (result != 0)
      return;

  GstBus *bus = gst_element_get_bus(mPipeline);
  if (bus == NULL) {
    tlog(TLOG_WARN, "get bus failed");
    gst_element_set_state(mPipeline, GST_STATE_NULL);
    gst_object_unref(mPipeline);
    sleep(1);
    mRestart = true;
    mElements.clear();
    return;
  }

  do {
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR |
                         GST_MESSAGE_EOS));

    if (msg == NULL)
      continue;
    GError *err;
    gchar *debugInfo;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debugInfo);
      tlog(TLOG_ERROR, "Error received from element %s: %s",
           GST_OBJECT_NAME(msg->src), err->message);
      tlog(TLOG_ERROR, "Debugging information: %s",
           debugInfo ? debugInfo : "none");
      g_clear_error(&err);
      g_free(debugInfo);
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
      }
      break;
    default:
      /* We should not reach here */
      tlog(TLOG_WARN, "Unexpected message received.");
      break;
    }
    gst_message_unref(msg);
  } while (!mRestart && !mQuit);

  gst_object_unref(bus);
  gst_element_set_state(mPipeline, GST_STATE_NULL);
  gst_object_unref(mPipeline);
  mElements.clear();
  tlog(TLOG_INFO, "thread end and quit normally");
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
    if (g_str_has_prefix(name, "H264")) {
      gst_pad_link(srcPad, sinkPad);
    } else {
      tlog(TLOG_WARN, "unsupported encode %s", name);
    }
  }

  gst_caps_unref(newPadCaps);
  gst_object_unref(sinkPad);
}
