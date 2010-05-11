/** \file ccnsink.c
 * \brief Implements the ccnx sink element for the GST plug-in
 *
 * \author John Letourneau <topgun@bell-labs.com>
 * \date Created: Dec, 2009
 */
/*
 * GStreamer-CCNx, CCNx sink element
 * Copyright (C) 2009, 2010 Alcatel-Lucent Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "conf.h"

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/netbuffer/gstnetbuffer.h>
#include <glib/gstdio.h>

#include "ccnxsink.h"
#include <ccn/keystore.h>
#include <ccn/signing.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include "utils.h"

/**
 * Delcare debugging structure types
 *
 * Use the <a href="http://www.gstreamer.net/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstInfo.html">GStreamer macro</a>
 * to declare some debugging stuff.
 */
GST_DEBUG_CATEGORY_STATIC (gst_ccnxsink_debug);

/**
 * Declare debugging stuff
 */
#define GST_CAT_DEFAULT gst_ccnxsink_debug

/**
 * Size of the CCN network blocks...sort of
 */
#define CCN_CHUNK_SIZE 4000
/**
 * Size of a FIFO block
 */
#define CCN_FIFO_BLOCK_SIZE (CCN_CHUNK_SIZE)
/**
 * Number of msecs prior to a get version() timeout
 */
#define CCN_VERSION_TIMEOUT 400
/**
 * I forget what this timeout is for
 */
#define CCN_HEADER_TIMEOUT 400


/**
 * Filter signals and args
 */
enum
{
  LAST_SIGNAL
};

/**
 * Element properties
 */
enum
{
  PROP_0, PROP_URI, PROP_SILENT
};

/**
 * Capabilities of the output sink.
 *
 */
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
                                                /**< Name of the element */
    GST_PAD_SINK,                       /**< Type of element, a sink */
    GST_PAD_ALWAYS,                     /**< We always have this PAD */
    GST_STATIC_CAPS_ANY);       /**< We can accept any kind of input */

/**
 * Describe the details for the GST tools to print and such.
 */
static const GstElementDetails gst_ccnxsink_details = GST_ELEMENT_DETAILS ("CCNX data sink",
                                                                        /**< Terse description */
    "Source/Network",                                                   /**< \todo ??? */
    "Publish data over a CCNx network",                 /**< Long description */
    "John Letourneau <topgun@bell-labs.com");      /**< Author contact information */

/**
 * Uses this port if no other port is specified
 */
#define CCNX_NETWORK_DEFAULT_PORT		1111

/**
 * Uses this address for the ccnd router if no other is specified
 */
#define CCNX_NETWORK_DEFAULT_ADDR		1.2.3.4

/**
 * Uses this default URI if no other one is specified
 */
#define CCNX_DEFAULT_URI			"ccnx:/error"

/**
 * We default to having the data never expire
 */
#define CCNX_DEFAULT_EXPIRATION     -1

/**
 * Signing parameters are initialized with this template
 */
static struct ccn_signing_params CCNX_DEFAULT_SIGNING_PARAMS =
    CCN_SIGNING_PARAMS_INIT;

/*
 * Several call-back function prototypes needed in the code below
 */
static void gst_ccnxsink_base_init (gpointer gclass);
static void gst_ccnxsink_class_init (GstccnxsinkClass * klass);
static void gst_ccnxsink_init (Gstccnxsink * me, GstccnxsinkClass * gclass);
static void gst_ccnxsink_finalize (GObject * object);
static gboolean gst_ccnxsink_start (GstBaseSink * bsrc);
static gboolean gst_ccnxsink_stop (GstBaseSink * sink);

static void gst_ccnxsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ccnxsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_ccnxsink_getcaps (GstBaseSink * src);

static GstFlowReturn gst_ccnxsink_publish (GstBaseSink * sink, GstBuffer * buf);

/**
 * The first function called which typically performs environmental initialization
 *
 * After the plug-in elements are registered, this would be the first initialization
 * function called. It is known to GST by virtue of appearing in the GST_BOILERPLATE_FULL
 * macro below.
 *
 * In our case, there are not initialization steps at this point.
 */
static void
_do_init ( /*@unused@ */ GType type)
{

}

/**
 * A macro to initialize key functions for the GStreamer initialization
 *
 * This
 * <a href="http://www.gstreamer.net/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstUtils.html#GST-BOILERPLATE-FULL:CAPS">GStreamer supplied macro</a>
 * is used to setup a type of function jump table for use by the GST framework.
 * It takes advantage of other macro definitions, and the fact that a naming convention is used in
 * the development of the plug-in...thus not every function GST needs must be enumerated.
 * The second parameter, gst_ccnxsink, is that function name prefix.
 * So the macro uses that to create entries for functions like:
 * \li gst_ccnxsink_base_init
 * \li gst_ccnssink_class_init
 * \li gst_ccnxsink_init
 * \li . . .
 *
 * The macro also takes as input the base class for our element; GST_TYPE_BASE_SINK.
 */
GST_BOILERPLATE_FULL (Gstccnxsink, gst_ccnxsink, GstBaseSink,
    GST_TYPE_BASE_SINK, _do_init);

/**
 * Perform basic \em linkage initialization with GST
 *
 * This is the second initialization function to be called, after _do_init().
 * Here is where the debug information is initialized, the class detailed description,
 * and the template pad is added to the element's context.
 *
 * \param gclass	pointer to the base class description, used in all registration functions used here
 */
static void
gst_ccnxsink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  GST_DEBUG_CATEGORY_INIT (gst_ccnxsink_debug, "ccnxsink", 0, "CCNx sink");

  gst_element_class_set_details (element_class, &gst_ccnxsink_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));
}

/**
 * initialize the ccnxsink's class
 *
 * This is the third initialization function called, after gst_ccnxsink_base_init().
 * Here is where all of the class specific information is established, like any static data
 * or functions.
 * Of particular interest are the functions for getting and setting a class instance parameters,
 * and the base class functions that control the lifecycle of those instances.
 *
 * \param klass		pointer to our class description
 */
static void
gst_ccnxsink_class_init (GstccnxsinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;

  GST_DEBUG ("CCNxSink: class init");

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  /* Point to the get/set functions for our properties */
  gobject_class->set_property = gst_ccnxsink_set_property;
  gobject_class->get_property = gst_ccnxsink_get_property;

  /* Register these properties, their names, and their help information */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI of the form: ccnx://<content name>", CCNX_DEFAULT_URI,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  /* Now setup the call-back functions for our lifecycle */
  gobject_class->finalize = gst_ccnxsink_finalize;
  gstbasesink_class->start = gst_ccnxsink_start;
  gstbasesink_class->stop = gst_ccnxsink_stop;
  gstbasesink_class->get_times = NULL;
  gstbasesink_class->get_caps = gst_ccnxsink_getcaps;
  gstbasesink_class->render = gst_ccnxsink_publish;     // Here in particular is where we process data from the pipeline
}

/**
 * Initialize the new element instance
 *
 * We instantiate our pad and add it to the element
 * We set the pad calback functions, and initialize instance data for our structure.
 * \todo shouldn't this code call the base class init also?
 *
 * \param me		pointer to the new instance
 * \param gclass	pointer to the class definition
 */
static void
gst_ccnxsink_init (Gstccnxsink * me,
    /*@unused@ */ GstccnxsinkClass * gclass)
{
  GST_DEBUG ("CCNxSink: instance init");

  gst_base_sink_set_sync (GST_BASE_SINK (me), FALSE);

  me->silent = FALSE;
  me->uri = g_strdup (CCNX_DEFAULT_URI);
  me->name = NULL;
  me->keylocator = NULL;
  me->keystore = NULL;
  me->ts = GST_CLOCK_TIME_NONE;
  me->temp = NULL;
  me->partial = NULL;
  me->lastPublish = NULL;
  me->signed_info = NULL;
  me->keylocator = NULL;
  me->keystore = NULL;
  memcpy (&(me->sp), &CCNX_DEFAULT_SIGNING_PARAMS,
      sizeof (CCNX_DEFAULT_SIGNING_PARAMS));
  me->expire = CCNX_DEFAULT_EXPIRATION;
  me->segment = 0;
  me->fifo_head = 0;
  me->fifo_tail = 0;
  me->buf = gst_buffer_new_and_alloc (CCN_FIFO_BLOCK_SIZE);
  me->obuf = NULL;

}

/**
 * Retrieve the capabilities of our pads
 *
 * GST can put a pipeline together automatically by finding src and sink
 * pads that have compatible capabilities. In order to find out what
 * those capabilities are, it knows to call this method to have returned
 * a structure of those salient attributes.
 *
 * \param sink		-> to an instance of this class
 * \return pointer to the capabilities structure
 */
static GstCaps *
gst_ccnxsink_getcaps (GstBaseSink * sink)
{
  Gstccnxsink *me;

  GST_DEBUG ("CCNxSink: get caps");

  me = GST_CCNXSINK (sink);     // Very common to see a cast to the proper structure type

  if (me->caps)
    return gst_caps_ref (me->caps);
  else
    return gst_caps_new_any ();
}

/**
 * test to see if a fifo queue is empty
 *
 * \param me	element context where the fifo is kept
 * \return true if the fifo is empty, false otherwise
 */
static gboolean
fifo_empty (Gstccnxsink * me)
{
  return me->fifo_head == me->fifo_tail;
}

/**
 * add an element to the fifo queue
 *
 * Each element has its own fifo queue to use when communicating with
 * the background task. When adding information to a full queue, we have
 * a choice of either throwing away some of the data, an overwrite condition,
 * or waiting for the other task to take something out of the queue
 * thus making some room for the new entry.
 *
 * A normal put operation takes place without the use of any locking mechanism.
 * If a full queue is detected, then a lock is used to verify the condition
 * and is also used to coordinate with the fifo_pop() which signifies an
 * un-full queue.
 *
 * \param me		element context where the fifo is kept
 * \param buf		the buffer we are to put on the queue
 * \param overwrite	flag indicating if we should throw away data [true] or wait [false] when the queue is full
 * \return true if the put succeeded, false otherwise
 */
static gboolean
fifo_put (Gstccnxsink * me, GstBuffer * buf, int overwrite)
{
  int next;

  GST_DEBUG ("CCNxSink: fifo putting");

  next = me->fifo_tail;
  if (++next >= CCNX_SINK_FIFO_MAX)
    next = 0;
  if (next == me->fifo_head) {
    g_mutex_lock (me->fifo_lock);
    if (overwrite) {
      if (next == me->fifo_head) {      /* repeat test under a lock */
        int h = me->fifo_head;
        if (++h >= CCNX_SINK_FIFO_MAX)
          h = 0;                /* throw the head one away */
        /* need to release the buffer prior to the overwrite, or memory will leak */
        gst_buffer_unref (me->fifo[me->fifo_head]);
        me->fifo_head = h;
        GST_LOG_OBJECT (me, "fifo put: overwriting a buffer");
      }
    } else {
      while (next == me->fifo_head) {
        GST_DEBUG ("FIFO: queue is full");
        g_cond_wait (me->fifo_cond, me->fifo_lock);
      }
    }
    g_mutex_unlock (me->fifo_lock);
    GST_DEBUG ("FIFO: queue is OK");
  }
  me->fifo[me->fifo_tail] = buf;
  me->fifo_tail = next;
  return TRUE;
}

/**
 * Retrieve the next element from the fifo queue
 *
 * This function is matched with the fifo_put() function.
 * It takes off the queue what the other puts onto the queue.
 * Currently all removals take place under a lock, and the
 * un-full condition is generated in the event the put code
 * may be waiting on the fifo.
 * \todo we can be smarter here and only use the lock if the queue is full.
 *
 * \param me		element context where the fifo is kept
 * \return buffer containing the next element, NULL if the queue is empty
 */
/*@null@*/
static GstBuffer *
fifo_pop (Gstccnxsink * me)
{
  GstBuffer *ans;
  int next;
  GST_DEBUG ("CCNxSink: fifo popping");
  if (fifo_empty (me)) {
    return NULL;
  }
  next = me->fifo_head;
  ans = me->fifo[next];
  if (++next >= CCNX_SINK_FIFO_MAX)
    next = 0;
  g_mutex_lock (me->fifo_lock);
  me->fifo_head = next;
  g_cond_signal (me->fifo_cond);
  g_mutex_unlock (me->fifo_lock);
  return ans;
}

#include <time.h>

/**
 * Helpful define to use in calculations
 */
#define A_THOUSAND    1000

/**
 * Helpful define to use in calculations
 */
#define A_MILLION     (A_THOUSAND*A_THOUSAND)

/**
 * Helpful define to use in calculations
 */
#define A_BILLION     (A_THOUSAND*A_MILLION)

/**
 * Returns the current time in GST format
 *
 * The GStreamer code has a specific definition of time.
 * As of this writing it is the number of nano-seconds since the epoch.
 * This function knows how to interface with the OS, and do the
 * proper scaling to get that time.
 *
 * \return A GST timestamp
 */
static GstClockTime
tNow ()
{
  struct timespec t;
  GstClockTime ans;

  clock_gettime (CLOCK_REALTIME, &t);
  ans = t.tv_sec;
  ans *= A_BILLION;
  ans += t.tv_nsec;
  return ans;
}

/**
 * Converts from one time specification to another
 *
 * Takes a GST time value and sets the timespec structure
 * appropriately to reflect that time. If the pointer to
 * the timespec is NULL, no operation is attempted.
 *
 * \param t		a GStreamer time value
 * \param ts	a timespec structure of the time value
 */
void
makeTimespec (GstClockTime t, struct timespec *ts)
{
  if (NULL == ts)
    return;
  ts->tv_sec = t / A_BILLION;
  ts->tv_nsec = t % A_BILLION;
}

/**
 * Send out a message onto the CCNx network
 *
 * Once a data buffer is ready to be sent onto the network, there are
 * a few more steps involved in preparing it for thta journey.
 * The CCN stack dictates that all data content messages must contain
 * a proper name, and be signed by the sender.
 * \todo some day we should also encrypt the data; I don't think it is now
 * Much of the information we need to accomplish the message preperation is
 * found in the element context or as one of the parameters.
 *
 * Most of the work is dealing with changing from the internal buffer size and
 * the message size of the CCN network.
 * We chop things up as needed and send each data message out, properly named and signed.
 * The naming will include the timestamp as well as a segment number.
 * If there are some partial bytes left over, that do not fill a CCN message packet,
 * we retain those to be sent out on the next call to this method.
 * The bytes are kept in the element context as is other pertinent information.
 *
 * \param me
 * \param data		pointer to the data buffer to send
 * \param size		number of bytes to send in the message
 * \param ts		timestamp to use on the outbound message
 * \return a GST flow return value indicating the result of our attempt
 * \retval GST_FLOW_OK everything went well
 * \retval GST_FLOW_ERROR something went wrong
 */
static GstFlowReturn
gst_ccnxsink_send (Gstccnxsink * me, guint8 * data, guint size, /*@unused@ */
    GstClockTime ts)
{
  struct ccn_charbuf *sname;    /* where we construct the name of this data message */
  struct ccn_charbuf *hold;     /* holds the last block published so we can properly manage memory */
  struct ccn_charbuf *temp;     /* where we construct the message to send */
  struct ccn_charbuf *signed_info;      /* signing data within the message */
  gint rc;                      /* return status on various calls */
  guint8 *xferStart;            /* points into the source buffer, data, as we packetize into CCN blocks */
  size_t bytesLeft;             /* keeps track of how much more we have to do */

  /* Initialize our local storage and allocate buffers we will need */
  xferStart = data;
  bytesLeft = size;
  sname = ccn_charbuf_create ();
  temp = ccn_charbuf_create ();
  signed_info = ccn_charbuf_create ();

  /* Hang onto this pointer so we can release the buffer as we exit this function */
  hold = me->lastPublish;

/*
 * I am hanging onto this code for now to handle the day when we want to encode the data.
 * I do not know why id did not work during development, but I am sure it is close to working 8-)
 * In the future, feel free to purge this if we never get around to using it.
 * Similarly for the code block that is down below a little bit.
 *
  if( me->keystore ) {
    signed_info->length = 0;
GST_LOG_OBJECT( me, "send - signing info\n" );
    rc = ccn_signed_info_create(signed_info
                                 , ccn_keystore_public_key_digest(me->keystore) //pubkeyid 
                                 , ccn_keystore_public_key_digest_length(me->keystore) //publisher_key_id_size 
                                 , NULL			//datetime
                                 , CCN_CONTENT_DATA	//type
                                 , me->expire		//freshness
                                 , NULL			//finalblockid
                                 , me->keylocator);
    // Put the keylocator in the first block only. 
    ccn_charbuf_destroy(&(me->keylocator));
    if (rc < 0) {
        GST_LOG_OBJECT(me, "Failed to create signed_info (rc == %d)\n", rc);
        goto Trouble;
    }
  }
	*/

  if (me->partial) {            /* We had some left over from the last send */
    size_t extra;
    uintmax_t seg;

    /* find out how much room we have left, and copy over the bytes we have, or need to fill the block */
    extra = CCN_CHUNK_SIZE - me->partial->length;
    if (extra > bytesLeft)
      extra = bytesLeft;
    GST_LOG_OBJECT (me, "send - had a partial left: %d\n", extra);
    ccn_charbuf_append (me->partial, xferStart, extra);

    /* Adjust count and pointer to reflect the bytes we have taken */
    bytesLeft -= extra;
    xferStart += extra;

    /* Filling to the size of the CCN packet means we need to send it out */
    if (me->partial->length == CCN_CHUNK_SIZE) {
      sname->length = 0;
      seg = me->segment++;

      /* build up a name starting with the prefix, then the sequence number */
      ccn_charbuf_append (sname, me->name->buf, me->name->length);
      ccn_name_append_numeric (sname, CCN_MARKER_SEQNUM, seg);
      temp->length = 0;

      /* Signing via this function does a lot of work. The result is a buffer, temp, that is ready to be sent */
      ccn_sign_content (me->ccn, temp, sname, &me->sp, me->partial->buf,
          CCN_CHUNK_SIZE);
      //  hDump( sname->buf, sname->length );
      /*
       * See the comment above about holding this code.
       *
       if( me->keystore ) {

       rc = ccn_encode_ContentObject(temp,
       sname,
       signed_info,
       me->partial->buf,
       CCN_CHUNK_SIZE,
       NULL,
       ccn_keystore_private_key(me->keystore));
       if (rc != 0) {
       GST_LOG_OBJECT( me, "Failed to encode ContentObject (rc == %d)\n", rc);
       goto Trouble;
       }
       */
      /* send the data message on its way */
      rc = ccn_put (me->ccn, temp->buf, temp->length);
      if (rc < 0) {
        GST_LOG_OBJECT (me, "ccn_put failed (rc == %d)\n", rc);
        goto Trouble;
      }
      /* free the buffer we used for the partial data */
      ccn_charbuf_destroy (&me->partial);
      me->partial = NULL;
/*
} else {
		GST_LOG_OBJECT( me, "No keystore. What should we do?\n" );
		goto Trouble;
	  }
*/
    }
  }

  /* No left over means we can send direct out of the data buffer */
  /* Now that we are done with the partial block, go and process the new data in much the same fashion */
  while (bytesLeft >= CCN_CHUNK_SIZE) {
    uintmax_t seg;
    GST_LOG_OBJECT (me, "send - bytesLeft: %d\n", bytesLeft);
    sname->length = 0;
    seg = me->segment++;
    ccn_charbuf_append (sname, me->name->buf, me->name->length);
    ccn_name_append_numeric (sname, CCN_MARKER_SEQNUM, seg);
    GST_LOG_OBJECT (me, "send - name is ready\n");
    temp->length = 0;

    ccn_sign_content (me->ccn, temp, sname, &me->sp, xferStart, CCN_CHUNK_SIZE);
    //  hDump( sname->buf, sname->length );
    /*
       if( me->keystore ) {

       GST_LOG_OBJECT( me, "send - encoding\n" );
       rc = ccn_encode_ContentObject(temp,
       sname,
       signed_info,
       xferStart,
       CCN_CHUNK_SIZE,
       NULL,
       ccn_keystore_private_key(me->keystore));
       if (rc != 0) {
       GST_LOG_OBJECT( me, "Failed to encode ContentObject (rc == %d)\n", rc);
       goto Trouble;
       }
     */
    GST_LOG_OBJECT (me, "send - putting\n");
    rc = ccn_put (me->ccn, temp->buf, temp->length);
    if (rc < 0) {
      GST_LOG_OBJECT (me, "ccn_put failed (rc == %d)\n", rc);
      goto Trouble;
    }
    /*
       } else {
       GST_LOG_OBJECT( me, "No keystore. What should we do?\n" );
       goto Trouble;
       }
     */
    GST_LOG_OBJECT (me, "send - adjusting buffers\n");
    /* msleep(5); */
    bytesLeft -= CCN_CHUNK_SIZE;
    xferStart += CCN_CHUNK_SIZE;
  }                             /* end of while() */

  if (bytesLeft) {              /* We have some left over for next time */
    GST_LOG_OBJECT (me, "send - for next time: %d\n", bytesLeft);
    me->partial = ccn_charbuf_create ();
    me->partial->length = 0;
    ccn_charbuf_append (me->partial, xferStart, bytesLeft);
  }

  /* Do proper memory management, then return */
  ccn_charbuf_destroy (&sname);
  me->lastPublish = temp;
  ccn_charbuf_destroy (&hold);
  ccn_charbuf_destroy (&signed_info);
  GST_LOG_OBJECT (me, "send - leaving length: %d\n", me->lastPublish->length);
  return GST_FLOW_OK;

Trouble:
  ccn_charbuf_destroy (&sname);
  ccn_charbuf_destroy (&temp);
  ccn_charbuf_destroy (&signed_info);
  return GST_FLOW_ERROR;
}

/**
 * Main render operating entry point for this sink element
 *
 * The GstBaseSinkClass#render value is setup to call this function where
 * there is data to be published on this sink pad. This function will do the
 * appropriate packetizing of the data, and then place it onto the
 * FIFO to be received by the background task that will deal with putting
 * it onto the CCNx network.
 *
 * \param sink		-> to our instance data
 * \param buffer	-> the data being rendered/published
 * \return indication of the success of failure of the operation
 * \retval GST_FLOW_OK if it went well
 * \retval GST_FLOW_ERROR not so good
 */
static GstFlowReturn
gst_ccnxsink_publish (GstBaseSink * sink, GstBuffer * buffer)
{
  Gstccnxsink *me;

  GST_DEBUG ("CCNxSink: publishing");

  me = GST_CCNXSINK (sink);

  gst_buffer_ref (buffer);
  fifo_put (me, buffer, TRUE);
  return GST_FLOW_OK;
}

/**
 * Call-back from the CCN network that something has arrived
 *
 * We get notified that a client has interests in things we are producing.
 * This function will do some basic checking to see what exactly the client has
 * an interest in, and will produce what is appropriate.
 * Currently the things we produce are:
 * \li last block - we resend the last block we have published for a given name
 * \li last segment - we produce a data message that only contains the latest segment number we used
 *
 * \param	selfp		-> a context structure we created when registering this call-back
 * \param	kind		specifies the type of call-back being processed, see the \b switch statement
 * \param	info		context information about the call-back itself; interests, data, etc.
 * \return a response as to how successful we were in processing the call-back
 * \retval CCN_UPCALL_RESULT_OK		things went well
 * \retval CCN_UPCALL_RESULT_VERIFY	need to verify the contents of what we received
 * \retval CCN_UPCALL_RESULT_REEXPRESS an interest timedout waiting for data, so we try again
 * \retval CCN_UPCALL_RESULT_ERR	some error was encountered
 */
static enum ccn_upcall_res
new_interests (struct ccn_closure *selfp,
    enum ccn_upcall_kind kind, struct ccn_upcall_info *info)
{
  Gstccnxsink *me = GST_CCNXSINK (selfp->data);
  struct ccn_charbuf *cb;
  struct ccn_charbuf *sname = NULL;
  const unsigned char *cp1, *cp2;
  size_t sz1;
  size_t sz2;
  long lastSeq;
  struct ccn_signing_params myparams;
  unsigned int i;
  int rc;


  GST_DEBUG ("something has arrived!");
  GST_DEBUG ("matched is: %d", info->matched_comps);    // number of filter components that were matched by the interest
  cb = interestAsUri (info);
  GST_DEBUG ("as URI: %s", ccn_charbuf_as_string (cb));
  ccn_charbuf_destroy (&cb);

  myparams = me->sp;

  /* Some debugging stuff */
  for (i = 0; i < 10; ++i) {
    const unsigned char *cp;
    size_t sz;
    GST_DEBUG ("%3d: ", i);
    if (0 > ccn_name_comp_get (info->interest_ccnb, info->interest_comps, i,
            &cp, &sz)) {
      // fprintf(stderr, "could not get comp\n");
      break;
    } else {
      // hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }
  }

  switch (kind) {

    case CCN_UPCALL_FINAL:
      GST_LOG_OBJECT (me, "CCN upcall final %p", selfp);
      return (0);

    case CCN_UPCALL_INTEREST_TIMED_OUT:
      if (selfp != me->ccn_closure) {
        GST_LOG_OBJECT (me, "CCN Interest timed out on dead closure %p", selfp);
        return (0);
      }
      GST_LOG_OBJECT (me, "CCN upcall reexpress -- timed out");
      if (me->timeouts > 5) {
        GST_LOG_OBJECT (me, "CCN upcall reexpress -- too many reexpressions");
        return (0);
      }
      me->timeouts++;
      return (CCN_UPCALL_RESULT_REEXPRESS);

    case CCN_UPCALL_CONTENT_UNVERIFIED:
      if (selfp != me->ccn_closure) {
        GST_LOG_OBJECT (me, "CCN unverified content on dead closure %p", selfp);
        return (0);
      }
      return (CCN_UPCALL_RESULT_VERIFY);

    case CCN_UPCALL_CONTENT:
      if (selfp != me->ccn_closure) {
        GST_LOG_OBJECT (me, "CCN content on dead closure %p", selfp);
        return (0);
      }
      break;

    case CCN_UPCALL_CONTENT_BAD:
      GST_LOG_OBJECT (me,
          "Content signature verification failed! Discarding.\n");
      return (CCN_UPCALL_RESULT_ERR);

    case CCN_UPCALL_CONSUMED_INTEREST:
      GST_LOG_OBJECT (me, "Upcall consumed interest\n");
      return (CCN_UPCALL_RESULT_ERR);   /* no data */

      /* Here is the most interesting case...when an interest arrives */
    case CCN_UPCALL_INTEREST:
      GST_INFO ("We got an interest\n");
      myparams.freshness = 1;   /* meta data is old very quickly */

      /* See if any meta information is sought */
      for (i = 0;; ++i) {
        if (0 > ccn_name_comp_get (info->interest_ccnb, info->interest_comps, i,
                &cp1, &sz1)) {
          cp1 = NULL;
          break;
        } else {
          if (!strncmp ((const char *) cp1, "_meta_", 6)) {     // OK, found meta, now which one is needed?
            if (0 > ccn_name_comp_get (info->interest_ccnb,
                    info->interest_comps, i + 1, &cp2, &sz2)) {
              GST_LOG_OBJECT (me,
                  "CCN interest received with invalid meta request");
              cp1 = NULL;
            }
            break;
          }                     // Component not meta, keep looking
        }
      }                         // At this point, i is left pointing at '_meta_' or at the end of component list

      if (cp1) {
        // hDump( DUMP_ADDR(cp1), DUMP_SIZE(sz1) );
        // hDump( DUMP_ADDR(cp2), DUMP_SIZE(sz2) );
        if (strncmp ((const char *) cp2, ".segment", 8))
          goto Exit_Interest;   /* not a match */

        /* publish what segment we are up to in reply to the meta request */
        lastSeq = me->segment - 1;
        GST_INFO ("sending meta data....segment: %d", lastSeq);

        sname = ccn_charbuf_create ();
        ccn_name_init (sname);
        rc = ccn_name_append_components (sname, info->interest_ccnb,
            info->interest_comps->buf[0], info->interest_comps->buf[i + 2]);
        if (rc < 0)
          goto Error_Interest;
        // rc = ccn_create_version(me->ccn, sname, CCN_V_REPLACE | CCN_V_NOW | CCN_V_HIGH, 0, 0);
        // if (rc < 0) goto Error_Interest;
        me->temp->length = 0;
        rc = ccn_sign_content (me->ccn, me->temp, sname, &myparams,
            &lastSeq, sizeof (lastSeq));
        // hDump(DUMP_ADDR(sname->buf), DUMP_SIZE(sname->length));
        if (rc != 0) {
          GST_LOG_OBJECT (me, "Failed to encode ContentObject (rc == %d)\n",
              rc);
          goto Error_Interest;
        }

        GST_INFO ("sending meta data...");
        // hDump(DUMP_ADDR(me->temp->buf), DUMP_SIZE(me->temp->length));
        rc = ccn_put (me->ccn, me->temp->buf, me->temp->length);
        me->temp->length = 0;
        if (rc < 0) {
          GST_LOG_OBJECT (me, "ccn_put failed (res == %d)\n", rc);
          goto Error_Interest;
        }
        GST_INFO ("meta data sent");

      } else
        goto Exit_Interest;     /* do not have _meta_ */

    Exit_Interest:
      ccn_charbuf_destroy (&sname);
      break;

    Error_Interest:
      ccn_charbuf_destroy (&sname);
      return CCN_UPCALL_RESULT_ERR;


    default:
      GST_LOG_OBJECT (me, "CCN upcall result error");
      return (CCN_UPCALL_RESULT_ERR);
  }


  me->timeouts = 0;


  return (CCN_UPCALL_RESULT_OK);

}

/**
 * Set up a connection with the ccnd router
 *
 * During this setup we establish a basic connection with the router service
 * over an IP connection.
 * This is also a good time to setup the ccn name for the content we will
 * produce; that is the prefix name and the timestamp [Tnow], since the
 * segment portion of the name is added just as the packet is being sent.
 *
 * Lastly we also load our security keys and create our signing parameters.
 *
 * \param me		context sink element for which the socket is for
 */
static void
setup_ccn (Gstccnxsink * me)
{
  struct ccn *ccn;

  GST_DEBUG ("CCNxSink: setup name...");
  if ((me->name = ccn_charbuf_create ()) == NULL) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL), ("name alloc failed"));
    return;
  }
  if (ccn_name_from_uri (me->name, me->uri) < 0) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL), ("name from uri failed"));
    return;
  }

  GST_DEBUG ("CCNxSink: creating ccn object");
  if ((ccn = ccn_create ()) == NULL) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL), ("ccn_create failed"));
    return;
  }
  me->ccn = ccn;
  GST_DEBUG ("CCNxSink: connecting");
  if (-1 == ccn_connect (me->ccn, ccndHost ())) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL), ("ccn_connect failed to %s",
            ccndHost ()));
    return;
  }

  GST_DEBUG ("CCNxSink: setting name version");
  if (0 > ccn_create_version (ccn, me->name,
          CCN_V_REPLACE | CCN_V_NOW | CCN_V_HIGH, 0, 0)) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL),
        ("ccn_create_version() failed"));
    return;
  }


  GST_DEBUG ("CCNxSink: setting up keystore");
  /*
     me->keystore = fetchStore();
     if( me->keystore ) me->keylocator = makeLocator( ccn_keystore_public_key(me->keystore) );
   */
  loadKey (me->ccn, &me->sp);
  GST_DEBUG ("CCNxSink: done; have keys!");
}

/**
 * Check if any work has appeared in the queue; work it if there
 *
 * The background task spends time waiting for something to do.
 * One of the places where work comes from is via the fifo queue,
 * which will contain data buffers that must be sent out over the ccn network.
 * This function will look for work and get it done if present.
 * Not too much work is donw however, since there are other things to be
 * done by the background task. Hence we limit the number of buffers will
 * will process from the queue.
 * We shall return soon enough to this spot to keep working the queue contents.
 *
 * \param me		context sink element where the fifo queues are allocated
 */
static void
check_fifo (Gstccnxsink * me)
{
  GstClockTime ts;
  gint i;
  guint size;
  guint8 *data;
  GstBuffer *buffer;

  for (i = 0; i < 3; ++i) {
    if (fifo_empty (me))
      return;
    if (!(buffer = fifo_pop (me)))
      return;
    size = GST_BUFFER_SIZE (buffer);
    data = GST_BUFFER_DATA (buffer);
    ts = 0;

    GST_INFO ("CCNxSink: pubish size: %d\n", size);
    if (0 == ts || GST_CLOCK_TIME_NONE == ts)
      ts = me->ts;
    if (0 == ts || GST_CLOCK_TIME_NONE == ts) {
      ts = tNow ();
      me->ts = ts;
    }

    GST_INFO ("CCNxSink: pubish time: %0X\n", ts);
    gst_ccnxsink_send (me, data, size, ts);
    gst_buffer_unref (buffer);
  }

}

static GstTask *eventTask;                              /**< -> to a GST task structure */
static GMutex *eventLock;                               /**< -> a lock that helps control the task */
static GCond *eventCond;                                /**< -> a condition structure to help with synchronization */
static GStaticRecMutex task_mutex               /**< I forget why we use this in this way */
    = G_STATIC_REC_MUTEX_INIT;

/**
 * Base loop for the background CCN task
 *
 * This is the main execution loop for the background task responsible for
 * interacting with the CCN network. It is from this point that many of the above methods are
 * called to work the inbound messages from ccnx as well as sending out the data messages.
 *
 * \param data		the task context information setup by the parent sink element thread
 */
static void
ccn_event_thread (void *data)
{
  Gstccnxsink *me = (Gstccnxsink *) data;
  struct ccn_charbuf *filtName;
  struct ccn_charbuf *temp;
  int res = 0;

  GST_DEBUG ("CCNxSink event: *** event thread starting");

  temp = ccn_charbuf_create ();
  filtName = ccn_charbuf_create ();

  /* A closure is what defines what to do when an inbound interest arrives */
  if ((me->ccn_closure = calloc (1, sizeof (struct ccn_closure))) == NULL) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL), ("closure alloc failed"));
    return;
  }

  /* We setup the closure to contain the sink element context reference, and also tell it what function to call */
  me->ccn_closure->data = me;
  me->ccn_closure->p = new_interests;
  me->timeouts = 0;
  ccn_charbuf_append (filtName, me->name->buf, me->name->length);

  /* This call will set up a handler for interests we expect to get from clients */

  // hDump(DUMP_ADDR(filtName->buf), DUMP_SIZE(filtName->length));
  ccn_set_interest_filter (me->ccn, filtName, me->ccn_closure);
  GST_DEBUG ("CCNxSink event: interest filter registered\n");

  /* Some debugging information */
  temp->length = 0;
  ccn_uri_append (temp, me->name->buf, me->name->length, TRUE);
  GST_DEBUG ("CCNxSink event: using uri: %s\n", ccn_charbuf_as_string (temp));

  /* Now that the interest is registered, we loop around waiting for something to do */
  /* We pass control to ccnx for a while so it can work with any incoming or outgoing data */
  /* and then we check our fifo queue for work to do. That's about it! */
  /* We check to see if any problems have caused our ccnd connection to fail, and we reconnect */
  while (res >= 0) {
    GST_DEBUG ("CCNxSink event: *** looping");
    res = ccn_run (me->ccn, 50);
    check_fifo (me);
    if (res < 0 && ccn_get_connection_fd (me->ccn) == -1) {
      GST_DEBUG ("CCNxSink event: need to reconnect...");
      /* Try reconnecting, after a bit of delay */
      msleep ((30 + (getpid () % 30)) * 1000);
      res = ccn_connect (me->ccn, ccndHost ());
    }
  }
  GST_DEBUG ("CCNxSink event: *** event thread ending");
}


/**
 * Tell the GST sink element it is time to prepare to do work
 *
 * This is one of the last functions GStreamer will call when the pipeline
 * is being put together. It is the last place the element has a chance to
 * allocate resources and in our case startup our background task for network
 * connectivity.
 * After this function returns, we are ready to start processing data from the pipeliine.
 *
 * We allocate some of the last minute buffers, and setup a connection to the network;
 * this is used primarily by the background task, but we need access to it for name initialization.
 *
 * Next we initialize our fifo queue, and startup the background task.
 *
 * Lastly we return to the GST to begin processing information.
 *
 * \param bsink		-> to the context sink element
 * \return true if the initialization went well and we are ready to process data, false otherwise
 */
static gboolean
gst_ccnxsink_start (GstBaseSink * bsink)
{
  Gstccnxsink *me;

  gboolean b_ret = FALSE;

  me = GST_CCNXSINK (bsink);
  me->temp = ccn_charbuf_create ();
  me->lastPublish = ccn_charbuf_create ();

  GST_DEBUG ("CCNxSink: starting, getting connections");

  GST_DEBUG ("CCNxSink: step 1, %s", me->uri);

  /* setup the connection to ccnx */
  setup_ccn (me);
  GST_DEBUG ("CCNxSink: ccn is setup");

  /* setup and start the background task */
  eventTask = gst_task_create (ccn_event_thread, me);
  if (NULL == eventTask) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL),
        ("creating event thread failed"));
    return FALSE;
  }
  me->fifo_cond = g_cond_new ();
  me->fifo_lock = g_mutex_new ();
  gst_task_set_lock (eventTask, &task_mutex);
  eventCond = g_cond_new ();
  eventLock = g_mutex_new ();
  b_ret = gst_task_start (eventTask);
  if (FALSE == b_ret) {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL),
        ("starting event thread failed"));
    return FALSE;
  }
  GST_DEBUG ("CCNxSink: event thread started");

  return TRUE;
}

/**
 * Stop processing packets and relinquish resources
 *
 * This should do the complimentary actions to undo what start did.
 * Currently we do nothing.
 *
 * \param bsink		element context that is being stopped
 * \return true if the stop worked properly, false otherwise
 */
static gboolean
gst_ccnxsink_stop (GstBaseSink * bsink)
{
  Gstccnxsink *me = GST_CCNXSINK (bsink);

  if (me->buf)
    fifo_put (me, me->buf, TRUE);

  GST_DEBUG ("stopping, closing connections");

  return TRUE;
}

/**
 * Set the context attribute for URI
 *
 * \param me		element context whose attribute is being set
 * \param gv		the value to which the attribute should be set
 * \return true if the value was set, false if some error occured
 */
static gboolean
gst_ccnxsink_set_uri (Gstccnxsink * me, const GValue * gv)
{
  gchar *protocol;
  const gchar *uri;

  /* cast to the proper type, then do some basic checks */
  uri = g_value_get_string (gv);
  GST_DEBUG ("CCNxSink: setting uri: %s\n", uri);

  if (NULL == uri) {
    g_free (me->uri);
    me->uri = g_strdup (CCNX_DEFAULT_URI);
    return TRUE;
  }

  /* we are specific as to what protocol can be used in the URI */
  protocol = gst_uri_get_protocol (uri);
  if (!protocol)
    goto wrong_protocol;
  if (strcmp (protocol, "ccnx") != 0)
    goto wrong_protocol;
  g_free (protocol);

  /* Free the old value before setting to the new attribute value */
  g_free (me->uri);
  me->uri = g_strdup (uri);

  return TRUE;

  /* ERRORS */
wrong_protocol:
  {
    GST_ELEMENT_ERROR (me, RESOURCE, READ, (NULL),
        ("error parsing uri %s: wrong protocol (%s != ccnx)", uri, protocol));
    g_free (protocol);
    return FALSE;
  }
}

/**
 * Sets a specified property to the given value
 *
 * The GST framework has a common way of handling the setting of element attributes.
 * We simply look at the attribute name to determine how to proceed.
 *
 * \param object		element context whose attribute is being modified
 * \param prop_id		value from our specified enumeration to know what attribute is changing
 * \param value			new value proposed by the user
 * \param pspec			\todo not sure what this is
 */
static void
gst_ccnxsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstccnxsink *me = GST_CCNXSINK (object);

  GST_DEBUG ("CCNxSink: set property: %d\n", prop_id);
  switch (prop_id) {
    case PROP_URI:
      gst_ccnxsink_set_uri (me, value);
      break;
    case PROP_SILENT:
      me->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Retrieve the current value of an attribute
 *
 * \param	object		element context whose attribute we are to return
 * \param prop_id		value from our specified enumeration to know what attribute is changing
 * \param value			value is returned via this pointer
 * \param pspec			\todo not sure what this is
 */
static void
gst_ccnxsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstccnxsink *me = GST_CCNXSINK (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, me->uri);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, me->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Release resources associated with this instance of the sink element
 *
 * This function compliments the gst_ccnxsink_init() function.
 * Think of it as a destructor for a class instance.
 *
 * \param obj		element context to have its resources released
 */
static void
gst_ccnxsink_finalize (GObject * obj)
{

  Gstccnxsink *me;

  me = GST_CCNXSINK (obj);

  if (me->caps)
    gst_caps_unref (me->caps);
  g_free (me->uri);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * \date Created: January, 2010
 * \author John Letourneau <topgun@bell-labs.com>
 *
 * \page CCNSINKDESIGN CCNx GStreamer Sink Design Unit
 *
 * This is primarily implemented in the ccnsink.c file;
 * others using these capabilities should include the ccnxsink.h file.
 * We also make use of functionality implemented in utils.c [utils.h].
 *
 * This GStreamer element is responsible for consuming packets from
 * a GST pipeline and delivering to them onto a Content Centric Network [CCNx]
 * for consumption by other clients wanting to read such published data;
 * via the partner element, ccnxsrc.
 *
 * \code
		|--------|---------|---------|----------| FIFO |-----------------|
		| elem-1 | elem-2  |  . . .  | ccnsink  | ---> | background task | ---> CCNx Network
		|--------|---------|---------|----------|      |-----------------|
 * \endcode
 * Here we depict the pipeline and where the ccnsink element fits in. As with most sink
 * elements it appears at the end of a potentially long pipeline; which may contain other
 * sink depending on how the user/application has configured the pipeline.
 * The \em ccnsink element will collect the pipeline data via a call-back to the
 * GstBaseSinkClass#render function we have implemented here as gst_ccnxsink_publish().
 * 
 * \section SINKCCNNAMING Naming CCN Data Content
 *
 * Each message being published on the CCN network is labled uniquely with a \em name.
 * As with most documentation of CCN naming, we use a URI notation that is readable by
 * humans, yet the internal representation does not look like a URI.
 * The prefix portion of the name is what most people would specify on the elements
 * attribute input.
 * For example:
 * \code
 *    ccnx://com/bell-labs/GC/development/John/laptop/camera/1
 * \endcode
 * Although being completely arbitrary, as far as CCNx is concerned,
 * we make use of rules and policies to assign a name to a thing that makes sense,
 * and can be administered at a global scale.
 * As such, our example is fairly precise in identifying one of the camera's on John's laptop.
 * John happens to be a developer in the Grand Challenge project.
 *
 * This name however is not quite sufficient when we consider that the camera may be used at
 * different times for transmitting media used in different ways.
 * We also need policies and rules that govern \em when the data was produced.
 * And so, this sink element is designed to assign a timestamp to the name that represents
 * when it was put into use for the given GST pipeline.
 * This \em start-point was felt as being a reasonable point in the camera's history
 * to represent all of the bytes generated until the pipeline would be taken down.
 *
 * Lastly there is the issue of taking this long stream of data and cutting it up
 * into managable chunks, or \em segments.
 * By numbering them consecutively, we define an ordering of the message blocks
 * that define a time sequence for presenting the data at the client port where the final
 * rendering will take place.
 * Thus as data is being produced, what the user does not see is the full name which contains
 * these added fields of timestamp and segment number:
 * \code
 *    ccnx://com/bell-labs/GC/development/John/laptop/camera/1/February 19, 2010, 14:22:19/4483
 * \endcode
 * This name, labeling a data block, shows it is the 4,483'rd segment in a stream of bytes from a camera
 * that started to broadcast at the indicated date and time.
 *
 * One last point about naming and the coordination between the producer of data and consomers
 * that express interest in the data.
 * This has to do with working with the real-time production of the information and the
 * client's desire to <em>join in</em> at the right time.
 * For a live stream of media, clients will often want to start viewing the stream in sync with
 * when it is being produced.
 * Some may desire starting at the beginning, but many will want to jump right in the middle.
 * In order to support this, a new client wishing to view the content of our \em .../camera/1
 * must discover additional information about the transmission.
 * The CCNx environment supports the ability to find the most current \em version of
 * a given prefixed name.
 * This yields a name with its timestamp suffix.
 * In order to learn of the segment information, the client must inquire from the producer of
 * the data what is current.
 *
 * To deal with this case, a special interest is used that identifies what meta-data from the
 * stream it wishes to learn about; in our case the current segment number.
 * This is data that is not typically published by the producer as it is sending out the media stream.
 * Rather the producer registers an interest filter with the network.
 * This filter specifies the prefix, the same used by the user, of the name for the camera/1 stream.
 * Now the client can ask for a more detailed information interest, and it will be routed to
 * the producer having that filter registered.
 * \code
 *    ccnx://com/bell-labs/GC/development/John/laptop/camera/1/_meta_/.segment
 * \endcode
 * Upon receipt, this sink element would locate the special \em __meta__ component of
 * the interest, and then see that the segment was needed.
 * Only at this time would this information be published,
 * and it would be marked as valid for only a very short amount of time...1 or 2 seconds.
 * This prevents other clients wishing to join the media stream,
 * expressing the same interest in the meta data,
 * from synchronizing with an outdated segment value.
 * At this time, only the segment meta data is available.
 *
 * \section SINKFIFOQUEUE FIFO Queue
 *
 * The following diagram depicts a FIFO queue capable of holding 6 elements.
 * New elements are put onto the queue at the \em tail [fifo_put()],
 * and read from the queue from the \em head [fifo_pop()].
 * The tail points at the last element added while the head points at the next element to be read.
 * When the tail and head coinside, the queue is empty;
 * when they differ by 1, as in the diagram, the queue is full.
 * \code
                      tail---\             /----head
                              \           /
                              V          V
        |--------|---------|---------|---------|--------|--------|
        |   n+8  |  n+9    |   n+10  |   n+5   |  n+6   |   n+7  |
        |--------|---------|---------|---------|--------|--------|
 * \endcode
 * The fifo is primarily a no-lock queue...however there is special treatment to deal with a queue full condition.
 * The use of separate \em head and \em tail index pointers is what makes this possible.
 * The put code will only use a lock when the queue is found to be full.
 * If the overwrite flag is true, the head/tail are adjusted in a way that the oldest [next to pop] item is discarded.
 * This has the effect of keeping the data producer moving at a cost of losing some data.
 * If the overwrite is false, then the producer will wait for space. The code performs the is-empty test under the
 * control of a lock, waiting on a condition to be signaled if it is still found to be full.
 * The pop code will modify its fifo index under the control of this same lock. It then always signals the condition
 * for which the put code might be waiting for.
 * This could be optimized by only sending the condition if the pop code finds it is going from a full to a non-full fifo.
 */
