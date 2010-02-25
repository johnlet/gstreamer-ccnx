/** \file ccnxsrc.h
 * \brief Implements the source element for a CCNx network
 *
 * \author John Letourneau <topgun@bell-labs.com>
 * \date Created: Nov, 2009
 */
 
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

 
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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

/**
 * Used as a shorthand to acquire the element type
 *
 * This is primarily used by the other defines from within this file.
 */
#define GST_TYPE_CCNXSRC \
  (gst_ccnxsrc_get_type())

/**
 * Used to cast a base type instance to a specialized type instance
 *
 * Development of GST plug-in elements typically involves sub-classing
 * one of the handy code objects GST comes with. Then the specialized
 * code takes care to call the base class methods and also do the
 * proper type casting in the various call-back functions. This is
 * a glib style of programming where your C code can take on C++
 * qualities, if you don't mind doing some extra work...like this
 * macro helps you do.
 *
 * In this specific case, we are working with an instance pointer and not
 * a class pointer.
 *
 * \param obj		a base instance type that is to be cast to a ccnxsrc element
 */
#define GST_CCNXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCNXSRC,Gstccnxsrc))

/**
 * Used to cast a base class to a specialized type class
 *
 * Development of GST plug-in elements typically involves sub-classing
 * one of the handy code objects GST comes with. Then the specialized
 * code takes care to call the base class methods and also do the
 * proper type casting in the various call-back functions. This is
 * a glib style of programming where your C code can take on C++
 * qualities, if you don't mind doing some extra work...like this
 * macro helps you do.
 *
 * In this case we are pointing to a class pointer and not an instance pointer.
 * So [I would expect] we would access static class methods using this call.
 *
 * \param klass		a base class type that is to be cast to a ccnxsrcclass
 */
#define GST_CCNXSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCNXSRC,GstccnxsrcClass))

/**
 * Checks to see if the object is of a given instance type
 *
 * \param obj		the object we are to test being a ccnxsrc element
 * \return true if it is, false otherwise
 */
#define GST_IS_CCNXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCNXSRC))

/**
 * Check to see if the object is a particular class type
 *
 * \param klass		the object we are to test being of a certain class
 * \return true if it is, false otherwise
 */
#define GST_IS_CCNXSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCNXSRC))

/**
 * Convenience definition
 */
typedef struct _Gstccnxsrc      Gstccnxsrc;

/**
 * Convenience definition
 */
typedef struct _GstccnxsrcClass GstccnxsrcClass;


/**
 * \brief Member data definition of our CCNx src element
 */
struct _Gstccnxsrc
{
  GstPushSrc	parent;					/**< We derive from this type of base class */

  GstPad		*srcpad;				/**< -> to our one and only pad definition */
  GstCaps		*caps;					/**< -> capabilities definition */

  gchar			*uri;					/**< URI we use to name the data we have interest in */
  long			i_seg;					/**< keeps track of what segment we need to ask for next */
  long			i_pos;					/**< keeps track of where we are in the stream of bytes coming in */
  long			i_bufoffset;			/**< keeps track of where we are in filling of the next pipeline buffer */
  int			timeouts;				/**< counts the number of interest timeouts we get before getting data */
  struct ccn	*ccn;					/**< handle to the ccn context with which we interact */
  struct ccn_closure *ccn_closure;		/**< defines the call-back information needed when ccn has something for us */
  struct ccn_signing_params sp;			/**< signing information used when we send interests out onto the network */
  struct ccn_charbuf *p_name;			/**< the ccn encoded name we show interest in */
  struct ccn_charbuf *p_template;		/**< the interest template used to hole key information */

  GMutex		*fifo_lock;				/**< used in some cases when adding/removing entries from the queue */
  GCond			*fifo_cond;				/**< used with the fifo_lock to wait for a queue to change from full to not full */
  GstBuffer*    buf;					/**< used in holding data moving between the network and the pipeline */
  GstBuffer*	fifo[CCNX_SRC_FIFO_MAX]; /**< the FIFO queue between the ccn network and the pipeline data delivery */
  int			fifo_head;				/**< index to the head of the FIFO queue; for the reader */
  int			fifo_tail;				/**< index to the tail of the FIFO queue; for the writer */

  gboolean		silent;					/**< an element attribute; currently not used */
};

/**
 * \brief Defines the element class
 *
 * No additional class level definition exists other than specifying
 * the base class from which this element type comes.
 */
struct _GstccnxsrcClass 
{
  GstPushSrcClass parent_class;			/**< base class type for the ccnx source element */
};

/**
 * Prototype for getting the type of an instance
 */
GType gst_ccnxsrc_get_type (void);

G_END_DECLS

#endif /* __CCNXSRC_H__ */
