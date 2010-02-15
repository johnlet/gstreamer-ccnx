/*
 * GStreamer, CCNx source element
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

#ifndef __CCNXSRC_H__
#define __CCNXSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/uri.h>
#include <ccn/header.h>


G_BEGIN_DECLS

#include <errno.h>
#include <string.h>

/* #defines don't like whitespacey bits */
#define GST_TYPE_CCNXSRC \
  (gst_ccnxsrc_get_type())
#define GST_CCNXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCNXSRC,Gstccnxsrc))
#define GST_CCNXSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCNXSRC,GstccnxsrcClass))
#define GST_IS_CCNXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCNXSRC))
#define GST_IS_CCNXSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCNXSRC))

typedef struct _Gstccnxsrc      Gstccnxsrc;
typedef struct _GstccnxsrcClass GstccnxsrcClass;

#define CCNX_FIFO_MAX	5

struct _Gstccnxsrc
{
  GstPushSrc	parent;

  GstPad	*sinkpad;
  GstPad	*srcpad;
  GstCaps	*caps;

  gchar		*uri;
  long    i_seg;
  long		i_size;
  long		i_pos;
  long		i_bufoffset;
  int		timeouts;
  struct ccn	*ccn;
  struct ccn_closure *ccn_closure;
  struct ccn_signing_params sp;
  struct ccn_charbuf *p_name;
  struct ccn_charbuf *p_template;

  GMutex	*fifo_lock;
  GCond		*fifo_cond;
  GstBuffer*    buf;
  GstBuffer*	fifo[CCNX_FIFO_MAX];
  int		fifo_head;
  int		fifo_tail;

  gboolean silent;
};

struct _GstccnxsrcClass 
{
  GstPushSrcClass parent_class;
};

GType gst_ccnxsrc_get_type (void);

G_END_DECLS

#endif /* __CCNXSRC_H__ */
