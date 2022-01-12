/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <gst/video/video.h>
#include <gst/video/video-format.h>

#include "uv4l2src.h"

#define DEFAULT_PROP_DEVICE   "/dev/video0"
#define DEFAULT_PROP_FORMAT   "YUYV"  // v4l2 videodev2.h

/* UV4l2Src signals and args */
enum
{
    /* FILL ME */
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_DEVICE,
    PROP_FORMAT,
    PROP_WIDTH,
    PROP_HEIGHT
};

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_uv4l2src_debug);
#define GST_CAT_DEFAULT gst_uv4l2src_debug

#define gst_uv4l2src_parent_class parent_class
G_DEFINE_TYPE (GstUV4l2Src, gst_uv4l2src, GST_TYPE_PUSH_SRC);

static void gst_uv4l2src_finalize (GObject * object);
static void gst_uv4l2src_set_property (GObject * object, guint prop_id,
        const GValue * value, GParamSpec * pspec);
static void gst_uv4l2src_get_property (GObject * object, guint prop_id,
        GValue * value, GParamSpec * pspec);

static GstCaps *gst_uv4l2src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_uv4l2src_set_caps (GstBaseSrc * bsrc,
    GstCaps * caps);
static GstCaps *gst_uv4l2src_get_caps (GstBaseSrc * bsrc,
    GstCaps * filter);
static gboolean gst_uv4l2src_start (GstBaseSrc * src);
static gboolean gst_uv4l2src_unlock (GstBaseSrc * bsrc);
static gboolean gst_uv4l2src_stop (GstBaseSrc * src);
static GstFlowReturn gst_uv4l2src_create (GstPushSrc * src, GstBuffer ** out);

static gboolean do_video_capture(GstUV4l2Src * src, GstBuffer * buf);

    static void
gst_uv4l2src_class_init (GstUV4l2SrcClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *element_class;
    GstBaseSrcClass *basesrc_class;
    GstPushSrcClass *pushsrc_class;

    gobject_class = (GObjectClass *) klass;
    basesrc_class = (GstBaseSrcClass *) klass;
    pushsrc_class = (GstPushSrcClass *) klass;
    element_class = (GstElementClass *) klass;

    gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_uv4l2src_set_property);
    gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_uv4l2src_get_property);
    gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_uv4l2src_finalize);

    g_object_class_install_property (gobject_class, PROP_DEVICE,
            g_param_spec_string ("device", "Device",
                "device name", DEFAULT_PROP_DEVICE, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_FORMAT,
            g_param_spec_string ("format", "Format",
                "v4l2 format", DEFAULT_PROP_FORMAT, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_WIDTH,
            g_param_spec_int ("width", "Width",
                "capture width", G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_HEIGHT,
            g_param_spec_int ("height", "Height",
                "capture height", G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));

    basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_uv4l2src_get_caps);
    basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_uv4l2src_set_caps);
    basesrc_class->start = GST_DEBUG_FUNCPTR (gst_uv4l2src_start);
    basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_uv4l2src_stop);
    basesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_uv4l2src_unlock);
    basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_uv4l2src_fixate);

    pushsrc_class->create = gst_uv4l2src_create;

    gst_element_class_add_static_pad_template (element_class, &srctemplate);

    gst_element_class_set_static_metadata (element_class, "UCloud v4l2 source",
            "Source/File",
            "read v4l2 camera", "Faicker Mo <faicker.mo@ucloud.cn>");

    GST_DEBUG_CATEGORY_INIT (gst_uv4l2src_debug, "uv4l2src", 0,
            "v4l2 camera source element");
}

    static void
gst_uv4l2src_init (GstUV4l2Src * src)
{
    src->fd = -1;
    src->buf_count = 0;
    src->device = strdup(DEFAULT_PROP_DEVICE);
    src->format = strdup(DEFAULT_PROP_FORMAT);
    src->is_capture = FALSE;
    gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
    gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
}

    static void
gst_uv4l2src_finalize (GObject * object)
{
    GstUV4l2Src *src = GST_UV4L2SRC (object);

    free(src->device);
    free(src->format);
    G_OBJECT_CLASS (parent_class)->finalize (object);
}


    static void
gst_uv4l2src_set_property (GObject * object, guint prop_id, const GValue * value,
        GParamSpec * pspec)
{
    GstUV4l2Src *src;

    src = GST_UV4L2SRC (object);

    switch (prop_id) {
        case PROP_DEVICE:
            free(src->device);
            src->device = g_value_dup_string (value);
            break;
        case PROP_FORMAT:
            free(src->format);
            src->format = g_value_dup_string (value);
            break;
        case PROP_WIDTH:
            src->width = g_value_get_int (value);
            break;
        case PROP_HEIGHT:
            src->height = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

    static void
gst_uv4l2src_get_property (GObject * object, guint prop_id, GValue * value,
        GParamSpec * pspec)
{
    GstUV4l2Src *src;

    g_return_if_fail (GST_IS_UV4L2SRC (object));
    src = GST_UV4L2SRC (object);

    switch (prop_id) {
        case PROP_DEVICE:
            g_value_set_string (value, src->device);
            break;
        case PROP_FORMAT:
            g_value_set_string (value, src->format);
            break;
        case PROP_WIDTH:
            g_value_set_int (value, src->width);
            break;
        case PROP_HEIGHT:
            g_value_set_int (value, src->height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

    static gboolean
gst_uv4l2src_unlock (GstBaseSrc * bsrc)
{
    GstUV4l2Src *src = GST_UV4L2SRC (bsrc);

    GST_OBJECT_LOCK (src);
    if (src->clock_id) {
        GST_DEBUG_OBJECT (src, "Waking up waiting clock");
        gst_clock_id_unschedule (src->clock_id);
    }
    GST_OBJECT_UNLOCK (src);

    return TRUE;
}

    static GstFlowReturn
gst_uv4l2src_create (GstPushSrc * push_src, GstBuffer ** buf)
{
    GstUV4l2Src *src = GST_UV4L2SRC (push_src);
    GstBuffer *new_buf;
    GstClock *clock;
    GstClockTime buf_time, buf_dur;
    guint64 frame_number;

    new_buf = gst_buffer_new_and_alloc(src->frame_size);

    clock = gst_element_get_clock (GST_ELEMENT (src));
    if (clock != NULL) {
        GstClockTime time, base_time;

        /* Calculate sync time. */

        time = gst_clock_get_time (clock);
        base_time = gst_element_get_base_time (GST_ELEMENT (src));
        buf_time = time - base_time;

        if (src->rate_numerator) {
            frame_number = gst_util_uint64_scale (buf_time,
                    src->rate_numerator, GST_SECOND * src->rate_denominator);
        } else {
            frame_number = -1;
        }
    } else {
        buf_time = GST_CLOCK_TIME_NONE;
        frame_number = -1;
    }

    if (frame_number != -1 && frame_number == src->frame_number) {
        GstClockID id;
        GstClockReturn ret;

        /* Need to wait for the next frame */
        frame_number += 1;

        /* Figure out what the next frame time is */
        buf_time = gst_util_uint64_scale (frame_number,
                src->rate_denominator * GST_SECOND, src->rate_numerator);

        id = gst_clock_new_single_shot_id (clock,
                buf_time + gst_element_get_base_time (GST_ELEMENT (src)));
        GST_OBJECT_LOCK (src);
        src->clock_id = id;
        GST_OBJECT_UNLOCK (src);

        GST_DEBUG_OBJECT (src, "Waiting for next frame time %" G_GUINT64_FORMAT,
                buf_time);
        ret = gst_clock_id_wait (id, NULL);
        GST_OBJECT_LOCK (src);

        gst_clock_id_unref (id);
        src->clock_id = NULL;
        if (ret == GST_CLOCK_UNSCHEDULED) {
            /* Got woken up by the unlock function */
            GST_OBJECT_UNLOCK (src);
            return GST_FLOW_FLUSHING;
        }
        GST_OBJECT_UNLOCK (src);

        /* Duration is a complete 1/fps frame duration */
        buf_dur =
            gst_util_uint64_scale_int (GST_SECOND, src->rate_denominator,
                    src->rate_numerator);
    } else if (frame_number != -1) {
        GstClockTime next_buf_time;

        GST_DEBUG_OBJECT (src, "No need to wait for next frame time %"
                G_GUINT64_FORMAT " next frame = %" G_GINT64_FORMAT " prev = %"
                G_GINT64_FORMAT, buf_time, frame_number, src->frame_number);
        next_buf_time = gst_util_uint64_scale (frame_number + 1,
                src->rate_denominator * GST_SECOND, src->rate_numerator);
        /* Frame duration is from now until the next expected capture time */
        buf_dur = next_buf_time - buf_time;
    } else {
        buf_dur = GST_CLOCK_TIME_NONE;
    }
    src->frame_number = frame_number;

    GST_BUFFER_TIMESTAMP (new_buf) = buf_time;
    GST_BUFFER_DURATION (new_buf) = buf_dur;

    do_video_capture (src, new_buf);
    *buf = new_buf;
    gst_object_unref (clock);
    return GST_FLOW_OK;
}

static gboolean do_video_capture(GstUV4l2Src * src, GstBuffer * buf) {
    GstMapInfo map_info;

    if (!src->is_capture) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(src->fd, VIDIOC_STREAMON, &type) == -1) {
            GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl capture failed"), NULL);
            return FALSE;
        }

        memset(&src->data_buf, 0, sizeof(src->data_buf));
        src->data_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        src->data_buf.memory = V4L2_MEMORY_MMAP;

        src->is_capture = TRUE;
    } else if (ioctl(src->fd, VIDIOC_QBUF, &src->data_buf) == -1) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_QBUF failed"), NULL);
        return FALSE;
    }

    while (ioctl(src->fd, VIDIOC_DQBUF, &src->data_buf) == -1) {
        if (errno == EIO)  {
            continue;
        }

        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_DQBUF failed"), NULL);
        return FALSE;
    }

    gst_buffer_map (buf, &map_info, GST_MAP_WRITE);
    memcpy (map_info.data, src->queue_buf[src->data_buf.index].data, map_info.size);
    gst_buffer_unmap (buf, &map_info);

    return TRUE;
}

    static GstCaps *
gst_uv4l2src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
    GstStructure *structure;

    caps = gst_caps_make_writable (caps);

    structure = gst_caps_get_structure (caps, 0);

    gst_structure_fixate_field_nearest_int (structure, "width", 640);
    gst_structure_fixate_field_nearest_int (structure, "height", 480);
    gst_structure_fixate_field_nearest_fraction (structure, "framerate", 24, 1);

    caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

    return caps;
}

    static gboolean
gst_uv4l2src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
    GstUV4l2Src *src = GST_UV4L2SRC (bsrc);
    GstStructure *structure;
    const GValue *framerate;

    structure = gst_caps_get_structure (caps, 0);

    framerate = gst_structure_get_value (structure, "framerate");
    if (framerate) {
        src->rate_numerator = gst_value_get_fraction_numerator (framerate);
        src->rate_denominator = gst_value_get_fraction_denominator (framerate);
    }

    GST_DEBUG_OBJECT (src, "v4l2 format %s, size %dx%d, %d/%d fps",
            src->format,
            src->width,
            src->height,
            src->rate_numerator, src->rate_denominator);

    return TRUE;
}

static GstVideoFormat
gst_v4l2_object_v4l2fourcc_to_video_format (guint32 fourcc)
{
  GstVideoFormat format;

  switch (fourcc) {
    case V4L2_PIX_FMT_GREY:    /*  8  Greyscale     */
      format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case V4L2_PIX_FMT_Y16:
      format = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case V4L2_PIX_FMT_Y16_BE:
      format = GST_VIDEO_FORMAT_GRAY16_BE;
      break;
    case V4L2_PIX_FMT_XRGB555:
    case V4L2_PIX_FMT_RGB555:
      format = GST_VIDEO_FORMAT_RGB15;
      break;
    case V4L2_PIX_FMT_XRGB555X:
    case V4L2_PIX_FMT_RGB555X:
      format = GST_VIDEO_FORMAT_BGR15;
      break;
    case V4L2_PIX_FMT_RGB565:
      format = GST_VIDEO_FORMAT_RGB16;
      break;
    case V4L2_PIX_FMT_RGB24:
      format = GST_VIDEO_FORMAT_RGB;
      break;
    case V4L2_PIX_FMT_BGR24:
      format = GST_VIDEO_FORMAT_BGR;
      break;
    case V4L2_PIX_FMT_XRGB32:
    case V4L2_PIX_FMT_RGB32:
      format = GST_VIDEO_FORMAT_xRGB;
      break;
    case V4L2_PIX_FMT_XBGR32:
    case V4L2_PIX_FMT_BGR32:
      format = GST_VIDEO_FORMAT_BGRx;
      break;
    case V4L2_PIX_FMT_ABGR32:
      format = GST_VIDEO_FORMAT_BGRA;
      break;
    case V4L2_PIX_FMT_ARGB32:
      format = GST_VIDEO_FORMAT_ARGB;
      break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      format = GST_VIDEO_FORMAT_NV12;
      break;
    case V4L2_PIX_FMT_NV12MT:
      format = GST_VIDEO_FORMAT_NV12_64Z32;
      break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
      format = GST_VIDEO_FORMAT_NV21;
      break;
    case V4L2_PIX_FMT_YVU410:
      format = GST_VIDEO_FORMAT_YVU9;
      break;
    case V4L2_PIX_FMT_YUV410:
      format = GST_VIDEO_FORMAT_YUV9;
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      format = GST_VIDEO_FORMAT_I420;
      break;
    case V4L2_PIX_FMT_YUYV:
      format = GST_VIDEO_FORMAT_YUY2;
      break;
    case V4L2_PIX_FMT_YVU420:
      format = GST_VIDEO_FORMAT_YV12;
      break;
    case V4L2_PIX_FMT_UYVY:
      format = GST_VIDEO_FORMAT_UYVY;
      break;
    case V4L2_PIX_FMT_YUV411P:
      format = GST_VIDEO_FORMAT_Y41B;
      break;
    case V4L2_PIX_FMT_YUV422P:
      format = GST_VIDEO_FORMAT_Y42B;
      break;
    case V4L2_PIX_FMT_YVYU:
      format = GST_VIDEO_FORMAT_YVYU;
      break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV16M:
      format = GST_VIDEO_FORMAT_NV16;
      break;
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_NV61M:
      format = GST_VIDEO_FORMAT_NV61;
      break;
    case V4L2_PIX_FMT_NV24:
      format = GST_VIDEO_FORMAT_NV24;
      break;
    default:
      format = GST_VIDEO_FORMAT_UNKNOWN;
      break;
  }

  return format;
}

    static GstCaps *
gst_uv4l2src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
    GstCaps *caps;
    GstUV4l2Src *src = GST_UV4L2SRC (bsrc);

    char a = src->format[0], b = src->format[1], c = src->format[2], d = src->format[3];
    guint32 pixelformat = v4l2_fourcc(a, b, c, d);
    GstVideoFormat fmt = gst_v4l2_object_v4l2fourcc_to_video_format(pixelformat);
    if (fmt == GST_VIDEO_FORMAT_UNKNOWN) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED,
                ("convert failed, unknown gst video format. v4l2 format %s", src->format), NULL);
        return NULL;
    }
    caps = gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string(fmt),
            "width", G_TYPE_INT, src->width,
            "height", G_TYPE_INT, src->height,
            "framerate", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);

    if (filter) {
        GstCaps *tmp =
            gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref (caps);
        caps = tmp;
    }

    return caps;
}

inline void unmap_buffer(GstUV4l2Src *src, int end) {
    for (int i = 0; i != end; ++i) {
        munmap(src->queue_buf[i].data, src->queue_buf[i].length);
    }
}

static gboolean
init_v4l2_buffer(GstUV4l2Src *src) {
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buf;

    reqbuf.count = 4;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(src->fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_REQBUFS failed"), NULL);
        return FALSE;
    }
    if (reqbuf.count == 0) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("count = 0"), NULL);
        return FALSE;
    }

    src->buf_count = reqbuf.count;

    for (int i = 0; i < src->buf_count; ++i) {
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(src->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_QUERYBUF failed"), NULL);
            return FALSE;
        }
       g_print("uv4l2src: frame size %d\n", buf.length);
        src->frame_size = buf.length;

        src->queue_buf[i].length = buf.length;
        src->queue_buf[i].data = (char*)mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, src->fd, buf.m.offset);

        if (src->queue_buf[i].data == MAP_FAILED) {
            GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("mmap failed"), NULL);
            unmap_buffer(src, i);
            return FALSE;
        }

        if (ioctl(src->fd, VIDIOC_QBUF, &buf) == -1) {
            GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("set buffer failed"), NULL);
            unmap_buffer(src, i);
            return FALSE;
        }
    }

    return TRUE;
}
/* open the file, necessary to go to RUNNING state */
    static gboolean
gst_uv4l2src_start (GstBaseSrc * bsrc)
{
    GstUV4l2Src *src = GST_UV4L2SRC (bsrc);
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    char a = src->format[0], b = src->format[1], c = src->format[2], d = src->format[3];

    src->fd = open(src->device, O_RDWR);
    if (src->fd < 0) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("open %s failed, %s", src->device, strerror(errno)), NULL);
        return FALSE;
    }
    if (ioctl(src->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_QUERYCAP failed, %s", strerror(errno)), NULL);
        return FALSE;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("no video capture"), NULL);
        return FALSE;
    }

    //fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //fmt.fmt.pix.width = src->width;
    //fmt.fmt.pix.height = src->height;
    //fmt.fmt.pix.pixelformat = v4l2_fourcc(a, b, c, d);
    //fmt.fmt.pix.field = V4L2_FIELD_NONE;

    //if (ioctl(src->fd, VIDIOC_S_FMT, &fmt) == -1) {
    //    GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("ioctl VIDIOC_S_FMT failed, %s", strerror(errno)), NULL);
    //    return FALSE;
    //}
    //if (fmt.fmt.pix.pixelformat != v4l2_fourcc(a, b, c, d)) {
    //    GST_ELEMENT_ERROR(src, RESOURCE, FAILED,
    //            ("unsupported format %s, cur is %u", src->format, fmt.fmt.pix.pixelformat), NULL);
    //    return FALSE;
    //}

    return init_v4l2_buffer(src);
}

/* close the file */
    static gboolean
gst_uv4l2src_stop (GstBaseSrc * bsrc)
{
    GstUV4l2Src *src = GST_UV4L2SRC (bsrc);

    if (src->fd > 0)
        close(src->fd);
    if (src->buf_count)
        unmap_buffer(src, src->buf_count);
    return TRUE;
}

    static gboolean
plugin_init (GstPlugin * plugin)
{
    return gst_element_register (plugin, "uv4l2src", GST_RANK_SECONDARY,
            GST_TYPE_UV4L2SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
        GST_VERSION_MINOR,
        uv4l2src,
        "read from v4l2 device",
        plugin_init, PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
