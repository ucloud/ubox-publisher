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


#ifndef __UV4L2SRC_H__
#define __UV4L2SRC_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <linux/videodev2.h>

G_BEGIN_DECLS

#define GST_TYPE_UV4L2SRC \
  (gst_uv4l2src_get_type())
#define GST_UV4L2SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_UV4L2SRC,GstUV4l2Src))
#define GST_UV4L2SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_UV4L2SRC,GstUV4l2SrcClass))
#define GST_IS_UV4L2SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_UV4L2SRC))
#define GST_IS_UV4L2SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_UV4L2SRC))

typedef struct _GstUV4l2Src GstUV4l2Src;
typedef struct _GstUV4l2SrcClass GstUV4l2SrcClass;

typedef struct {
    char *data;
    size_t length;
} __QBuf ;

struct _GstUV4l2Src {
  GstPushSrc parent_object;
  gchar *device;
  gint width, height;
  gchar *format;         // v4l2 format, videodev2.h
  gint rate_numerator;
  gint rate_denominator;
  gint fd;

  guint64 frame_number;
  GstClockID clock_id;

  gboolean is_capture;
  int frame_size;
  int buf_count;
  __QBuf queue_buf[4];
  struct v4l2_buffer data_buf;
};

struct _GstUV4l2SrcClass {
  GstPushSrcClass parent_class;
};

GType gst_uv4l2src_get_type (void);

G_END_DECLS

#endif /* __UV4L2SRC_H__ */
