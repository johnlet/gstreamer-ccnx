/** \file ccnxsink.h
 * \brief Implements the sink element for a CCNx network
 *
 * \author John Letourneau <topgun@bell-labs.com>
 * \date Created: Dec, 2009
 */

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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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

/**
 * Used as a shorthand to acquire the element type
 *
 * This is primarily used by the other defines from within this file.
 */
#define GST_TYPE_CCNXSINK \
  (gst_ccnxsink_get_type())

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
 * \param obj		a base instance type that is to be cast to a ccnxsink element
 */
#define GST_CCNXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCNXSINK,Gstccnxsink))

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
 * \param klass		a base class type that is to be cast to a ccnxsinkclass
 */
#define GST_CCNXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCNXSINK,GstccnxsinkClass))

/**
 * Checks to see if the object is of a given instance type
 *
 * \param obj		the object we are to test being a ccnxsink element
 * \return true if it is, false otherwise
 */
#define GST_IS_CCNXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCNXSINK))

/**
 * Check to see if the object is a particular class type
 *
 * \param klass		the object we are to test being of a certain class
 * \return true if it is, false otherwise
 */
#define GST_IS_CCNXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCNXSINK))

/**
 * Convenience definition
 */
typedef struct _Gstccnxsink      Gstccnxsink;

/**
 * Convenience definition
 */
typedef struct _GstccnxsinkClass GstccnxsinkClass;


/**
 * \brief Member data definition of our CCNx sink element
 */
struct _Gstccnxsink
{
  GstBaseSink		 parent;		/**< We derive from this base class */

  GstPad			 *sinkpad;		/**< -> to our one and only pad definition */
  GstCaps			 *caps;			/**< -> capabilities definition */

  gchar				 *uri;			/**< URI we use to name the data we publish */
  struct ccn_charbuf *name;			/**< URI converted to a name */
  
  int				 timeouts;		/**< Keeps track of interests we submit that timeout; we give up if too many */
  struct ccn		 *ccn;			/**< -> to our ccn handle, needed in all ccn calls */
  struct ccn_closure *ccn_closure;	/**< -> defines the call-back from ccn when interests arrive */
  struct ccn_charbuf *p_template;	/**< -> template for interests expressed within ccn network */
  struct ccn_charbuf *temp;			/**< -> temporary character buffer */
  struct ccn_charbuf *partial;		/**< -> buffer used to retain outbound message data until we have enough to send */
  struct ccn_charbuf *lastPublish;	/**< -> last buffer of data we published, in case we get an interest */
  struct ccn_charbuf *signed_info;	/**< -> our signature information for preparing our data to publish */
  struct ccn_charbuf *keylocator;	/**< -> our key information encoded for inclusion into our published data */
  struct ccn_keystore *keystore;	/**< -> our security keys information */
  struct ccn_signing_params sp;		/**< used when preparing our data to be published */
  long    expire;					/**< keeps the time we label our data for expiration; very small for streaming data */
  long    segment;					/**< keeps track of what segment of data we have published up to */

  GMutex	*fifo_lock;				/**< used in some cases when adding/removing entries from the queue */
  GCond		*fifo_cond;				/**< used with the fifo_lock to wait for a queue to change from full to not full */
  GstClockTime ts;					/**< the timestamp we are using to label all of our published data */
  GstBuffer* buf;					/**< assembles sink buffers into fifo buffers before queuing */
  GstBuffer* obuf;					/**< hold the buffer, from the fifo, being sent out as CCN packets */
  GstBuffer* fifo[CCNX_SINK_FIFO_MAX]; /**< the FIFO queue between the pipeline and the ccn network data delivery */
  int		fifo_head;				/**< index to the head of the FIFO queue; for the reader */
  int		fifo_tail;				/**< index to the tail of the FIFO queue; for the writer */

  gboolean silent;					/**< an element attribute; currently not used */
};

/**
 * \brief Defines the element class
 *
 * No additional class level definition exists other than specifying
 * the base class from which this element type comes.
 */
struct _GstccnxsinkClass 
{
  GstBaseSinkClass parent_class;	/**< base class type for the ccnx sink element */
};

/**
 * Prototype for getting the type of an instance
 */
GType gst_ccnxsink_get_type (void);

G_END_DECLS

#endif /* __CCNXSINK_H__ */
