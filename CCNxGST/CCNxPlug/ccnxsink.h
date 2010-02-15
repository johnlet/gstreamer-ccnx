/*
 * GStreamer, CCNx sink element
 * Copyright (C) 2009 John Letourneau <topgun@bell-labs.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __CCNXSINK_H__
#define __CCNXSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/uri.h>
#include <ccn/header.h>


G_BEGIN_DECLS

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* #defines don't like whitespacey bits */
#define GST_TYPE_CCNXSINK \
  (gst_ccnxsink_get_type())
#define GST_CCNXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCNXSINK,Gstccnxsink))
#define GST_CCNXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCNXSINK,GstccnxsinkClass))
#define GST_IS_CCNXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCNXSINK))
#define GST_IS_CCNXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCNXSINK))

typedef struct _Gstccnxsink      Gstccnxsink;
typedef struct _GstccnxsinkClass GstccnxsinkClass;

#define CCNX_FIFO_MAX	20

struct _Gstccnxsink
{
  GstBaseSink parent;

  GstPad	*sinkpad;
  GstCaps	*caps;

  gchar		*uri;
  struct ccn_charbuf *name;
  
  long		i_size;
  long		i_pos;
  long		i_bufoffset;
  long    o_bufoffset;
  int		timeouts;
  struct ccn	*ccn;
  struct ccn_closure *ccn_closure;
  struct ccn_charbuf *p_template;
  struct ccn_charbuf *temp;
  struct ccn_charbuf *partial;
  struct ccn_charbuf *lastPublish;
  struct ccn_charbuf *signed_info;
  struct ccn_charbuf *keylocator;
  struct ccn_keystore *keystore;
  struct ccn_signing_params sp;
  long    expire;
  long    segment;

  GMutex	*fifo_lock;
  GCond		*fifo_cond;
  GstClockTime ts;
  GstBuffer*    buf;    /* assembles sink buffers into fifo buffers before queuing */
  GstBuffer*    obuf;   /* hold the buffer, from the fifo, being sent out as CCN packets */
  GstBuffer*	fifo[CCNX_FIFO_MAX];
  int		fifo_head;
  int		fifo_tail;

  gboolean silent;
};

struct _GstccnxsinkClass 
{
  GstBaseSinkClass parent_class;
};

GType gst_ccnxsink_get_type (void);

G_END_DECLS

#endif /* __CCNXSINK_H__ */
