/** \file ccnxsrc.h
 * \brief Implements the source element for a CCNx network
 *
 * \author John Letourneau <topgun@bell-labs.com>
 * \date Created: Nov, 2009
 */
/*
 * GStreamer-CCNx, interface GStreamer media flow with a CCNx network
 * Copyright (C) 2009 Alcatel-Lucent Inc, and John Letourneau <topgun@bell-labs.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation;
 * version 2 of the License.
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
 * You should have received a copy of the GNU Library General Public
 * License, License.txt, along with this library; if not, write to the Free
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
 * Convenience definition
 */
typedef struct _CcnxInterestState CcnxInterestState;
typedef enum _OInterestState OInterestState;

/**
 * Outstanding interest have one of these states
 */
enum _OInterestState {
	OInterest_0					/**< Invalud state */
	, OInterest_idle			/**< Indicator that this state is available for use by a new interest */
	, OInterest_waiting			/**< We are waiting for the interest to be answered */
	, OInterest_timeout			/**< This interest has timed out */
	, OInterest_havedata		/**< Data has arrived for this interest */
};

/**
 * \brief Maintains information about outstanding interests
 */
struct _CcnxInterestState {
	OInterestState		state;			/**< State of this outstanding interest */
	uintmax_t			seg;			/**< segment number we are waiting for */
	gboolean			lastBlock;		/**< flag indicating this is the last segment we will get */
	guchar				*data;			/**< where the data is being held */
	size_t				size;			/**< how much data we have */
	gint				timeouts;		/**< count of how many times we asked for this data */
};


/**
 * \brief Member data definition of our CCNx src element
 */
struct _Gstccnxsrc
{
  GstPushSrc	parent;					/**< We derive from this type of base class */

  GstPad		*srcpad;				/**< -> to our one and only pad definition */
  GstCaps		*caps;					/**< -> capabilities definition */

  gchar			*uri;					/**< URI we use to name the data we have interest in */
  gint			intWindow;				/**< count of outstanding interests we have */
  CcnxInterestState *intStates;			/**< array of outstanding interests state structures */
  uintmax_t		post_seg;				/**< keeps track of what segment we need to post to the pipeline next */
  uintmax_t		i_seg;					/**< keeps track of what segment we need to ask for next */
  size_t		i_pos;					/**< keeps track of where we are in the stream of bytes coming in */
  size_t		i_bufoffset;			/**< keeps track of where we are in filling of the next pipeline buffer */
  struct ccn	*ccn;					/**< handle to the ccn context with which we interact */
  struct ccn_closure *ccn_closure;		/**< defines the call-back information needed when ccn has something for us */
  struct ccn_signing_params sp;			/**< signing information used when we send interests out onto the network */
  struct ccn_charbuf *p_name;			/**< the ccn encoded name we show interest in */
  struct ccn_charbuf *p_template;		/**< the interest template used to hole key information */

  GMutex		*fifo_lock;				/**< used in some cases when adding/removing entries from the queue */
  GCond			*fifo_cond;				/**< used with the fifo_lock to wait for a queue to change from full to not full */
  GstBuffer*    buf;					/**< used in holding data moving between the network and the pipeline */
  GstBuffer*	fifo[CCNX_SRC_FIFO_MAX]; /**< the FIFO queue between the ccn network and the pipeline data delivery */
  gint			fifo_head;				/**< index to the head of the FIFO queue; for the reader */
  gint			fifo_tail;				/**< index to the tail of the FIFO queue; for the writer */

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
