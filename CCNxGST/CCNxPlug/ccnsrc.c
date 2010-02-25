/** \file ccnsrc.c
 * \brief Implements the ccnx source element for the GST plug-in
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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/netbuffer/gstnetbuffer.h>

#include "ccnxsrc.h"
#include "utils.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>
#include <stdlib.h>

/**
 * Delcare debugging structure types
 *
 * Use the <a href="http://www.gstreamer.net/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstInfo.html">GStreamer macro</a>
 * to declare some debugging stuff.
 */
GST_DEBUG_CATEGORY_STATIC (gst_ccnxsrc_debug);

/**
 * Declare debugging stuff
 */
#define GST_CAT_DEFAULT gst_ccnxsrc_debug

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
  /* FILL ME */
  LAST_SIGNAL
};


/**
 * Element properties
 */
enum
{
  PROP_0
  , PROP_URI
  , PROP_SILENT
};

/**
 * Capabilities of the input source.
 *
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE (
	"src",					/* name of the element */
    GST_PAD_SRC,			/* type of pad provided */
    GST_PAD_ALWAYS,			/* lifetime of the pad */
    GST_STATIC_CAPS_ANY		/* kinds of things that this pad produces */
    );

/**
 * Describe the details for the GST tools to print and such.
 */
static const GstElementDetails gst_ccnxsrc_details = GST_ELEMENT_DETAILS(
	"CCNX interest source",				/* terse description of the element */
	"Source/Network",					/* \todo ?? */
	"Receive data over a CCNx network via interests being sent out", /* full description */
	"John Letourneau <topgun@bell-labs.com" ); /* Author and contact information */

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

/*
 * Several call-back function prototypes needed in the code below
 */
static void gst_ccnxsrc_uri_handler_init (gpointer g_iface, gpointer iface_data);

static void gst_ccnxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ccnxsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps * gst_ccnxsrc_getcaps (GstBaseSrc * src);

static GstFlowReturn gst_ccnxsrc_create (GstBaseSrc * psrc, guint64 offset, guint size, GstBuffer ** buf);

static gboolean gst_ccnxsrc_start (GstBaseSrc * bsrc);

static gboolean gst_ccnxsrc_stop (GstBaseSrc * bsrc);

static gboolean gst_ccnxsrc_unlock (GstBaseSrc * bsrc);

static gboolean gst_ccnxsrc_unlock_stop (GstBaseSrc * bsrc);

static enum ccn_upcall_res incoming_content(struct ccn_closure *selfp, enum ccn_upcall_kind kind,
                 struct ccn_upcall_info *info);

static void gst_ccnxsrc_finalize (GObject * object);

/**
 * The first function called which typically performs environmental initialization
 *
 * After the plug-in elements are registered, this would be the first initialization
 * function called. It is known to GST by virtue of appearing in the GST_BOILERPLATE_FULL
 * macro below.
 *
 * In our case, the pad sets up itself as a URI handler. The interface allows for
 * auto-matching of capabilities across elements.
 *
 * \param type		\todo not quite sure what this is; code was copied
 */
static void
_do_init (GType type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_ccnxsrc_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &urihandler_info);

}

/**
 * A macro to initialize key functions for the GStreamer initialization
 *
 * This
 * <a href="http://www.gstreamer.net/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstUtils.html#GST-BOILERPLATE-FULL:CAPS">GStreamer supplied macro</a>
 * is used to setup a type of function jump table for use by the GST framework.
 * It takes advantage of other macro definitions, and the fact that a naming convention is used in
 * the development of the plug-in...thus not every function GST needs must be enumerated.
 * The second parameter, gst_ccnxsrc, is that function name prefix.
 * So the macro uses that to create entries for functions like:
 * \li gst_ccnxsrc_base_init
 * \li gst_ccnssrc_class_init
 * \li gst_ccnxsrc_init
 * \li . . .
 *
 * The macro also takes as input the base class for our element; GST_TYPE_PUSH_SRC.
 */
GST_BOILERPLATE_FULL (Gstccnxsrc, gst_ccnxsrc, GstPushSrc,
    GST_TYPE_PUSH_SRC, _do_init);

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
gst_ccnxsrc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);
  
  GST_DEBUG_CATEGORY_INIT (gst_ccnxsrc_debug, "ccnxsrc",
      0, "CCNx src");

  gst_element_class_set_details(element_class,
     &gst_ccnxsrc_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

/**
 * initialize the ccnxsrc's class
 *
 * This is the third initialization function called, after gst_ccnxsrc_base_init().
 * Here is where all of the class specific information is established, like any static data
 * or functions.
 * Of particular interest are the functions for getting and setting a class instance parameters,
 * and the base class functions that control the lifecycle of those instances.
 *
 * \param klass		pointer to our class description
 */
static void
gst_ccnxsrc_class_init (GstccnxsrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  
	GST_DEBUG("CCNxSrc: class init");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;

  /* Point to the get/set functions for our properties */
  gobject_class->set_property = gst_ccnxsrc_set_property;
  gobject_class->get_property = gst_ccnxsrc_get_property;
  gobject_class->finalize = gst_ccnxsrc_finalize;

  /* Register these properties, their names, and their help information */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI of the form: ccnx://<content name>",
          CCNX_DEFAULT_URI, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  /* Now setup the call-back functions for our lifecycle */
  gstbasesrc_class->start = gst_ccnxsrc_start;
  gstbasesrc_class->stop = gst_ccnxsrc_stop;
  gstbasesrc_class->unlock = gst_ccnxsrc_unlock;
  gstbasesrc_class->unlock_stop = gst_ccnxsrc_unlock_stop;
  gstbasesrc_class->get_caps = gst_ccnxsrc_getcaps;
  gstbasesrc_class->create = gst_ccnxsrc_create; // Here in particular is the function used when the pipeline wants more data
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
gst_ccnxsrc_init (Gstccnxsrc * me,
    GstccnxsrcClass * gclass)
{

  me->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (me->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (me), me->srcpad);

  me->silent = FALSE;
  me->uri = g_strdup(CCNX_DEFAULT_URI);
  me->fifo_head = 0;
  me->fifo_tail = 0;
  me->i_pos = 0;
  me->i_bufoffset = 0;
  me->buf = gst_buffer_new_and_alloc(CCN_FIFO_BLOCK_SIZE);

  gst_base_src_set_format( GST_BASE_SRC(me), GST_FORMAT_TIME );
  gst_base_src_set_do_timestamp( GST_BASE_SRC(me), TRUE );
}

/**
 * Set the context attribute for URI
 *
 * \param me		element context whose attribute is being set
 * \param uri		the value to which the attribute should be set
 * \return true if the value was set, false if some error occured
 */
static gboolean
gst_ccnxsrc_set_uri (Gstccnxsrc * me, const gchar * uri)
{
  gchar *protocol;
  gchar *location;
  gchar *colptr;

  /* cast to the proper type, then do some basic checks */
  /* we are specific as to what protocol can be used in the URI */
  protocol = gst_uri_get_protocol (uri);
  if (strcmp (protocol, "ccnx") != 0)
    goto wrong_protocol;
  g_free (protocol);

  location = gst_uri_get_location (uri);
  if (!location)
    return FALSE;
  colptr = strrchr (location, ':');
  g_free (location);

  /* Free the old value before setting to the new attribute value */
  g_free (me->uri);
  me->uri = g_strdup(uri);

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
gst_ccnxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstccnxsrc *me = GST_CCNXSRC (object);

  switch (prop_id) {
    case PROP_URI:
      g_free (me->uri);
      if( g_value_get_string (value) == NULL )
        me->uri = g_strdup(CCNX_DEFAULT_URI);
      else
        me->uri = g_value_dup_string(value);
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
gst_ccnxsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstccnxsrc *me = GST_CCNXSRC (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string(value, me->uri);
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
 * Retrieve the capabilities of our pads
 *
 * GST can put a pipeline together automatically by finding src and sink
 * pads that have compatible capabilities. In order to find out what
 * those capabilities are, it knows to call this method to have returned
 * a structure of those salient attributes.
 *
 * \param src		-> to an instance of this class
 * \return pointer to the capabilities structure
 */
static GstCaps *
gst_ccnxsrc_getcaps (GstBaseSrc * src)
{
  Gstccnxsrc *me;

  me = GST_CCNXSRC (src);

  if (me->caps)
    return gst_caps_ref (me->caps);
  else
    return gst_caps_new_any ();
}

/* GstElement vmethod implementations */

/*
 * As a source, we do not have a set caps method
 */

/*
 * As a source, we do not have a chain method
 */

/**
 * Release resources associated with this instance of the source element
 *
 * This function compliments the gst_ccnxsrc_init() function.
 * Think of it as a destructor for a class instance.
 *
 * \param obj		element context to have its resources released
 */
static void
gst_ccnxsrc_finalize(GObject *obj) {

  Gstccnxsrc *me;

  me = GST_CCNXSRC (obj);

  if (me->caps)
    gst_caps_unref (me->caps);
  g_free (me->uri);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * Create a name with a sequence number component
 *
 * \param basename	the prefix, possibly versioined with a timestamp, to be used in the new name
 * \param seq		the sequence number to add to the prefix
 * \return character buffer with the new name
 */
static struct ccn_charbuf *
sequenced_name(struct ccn_charbuf *basename, uintmax_t seq)
{
    struct ccn_charbuf *name = NULL;

    name = ccn_charbuf_create();
    ccn_charbuf_append_charbuf(name, basename);
    ccn_name_append_numeric(name, CCN_MARKER_SEQNUM, seq);
    return(name);
}

/**
 * test to see if a fifo queue is empty
 *
 * \param me	element context where the fifo is kept
 * \return true if the fifo is empty, false otherwise
 */
static gboolean
fifo_empty( Gstccnxsrc* me ) {
    return me->fifo_head == me->fifo_tail;
}

/**
 * add an element to the fifo queue
 *
 * Each element has its own fifo queue to use when communicating with
 * the background task. When adding information to a full queue, we block
 * waiting for the other task to take something out of the queue
 * thus making some room for the new entry.
 *
 * A normal put operation takes place without the use of any locking mechanism.
 * If a full queue is detected, then a lock is used to verify the condition
 * and is also used to coordinate with the fifo_pop() which signifies an
 * un-full queue.
 *
 * \param me		element context where the fifo is kept
 * \param buf		the buffer we are to put on the queue
 * \return true if the put succeeded, false otherwise
 */
static gboolean
fifo_put(  Gstccnxsrc* me, GstBuffer *buf ) {
    int next;
        GST_DEBUG("FIFO: putting");
    next = me->fifo_tail;
    if( ++next >= CCNX_SRC_FIFO_MAX ) next = 0;
    if( next  == me->fifo_head ) {
        g_mutex_lock( me->fifo_lock );
        while( next  == me->fifo_head ) {
            GST_DEBUG("FIFO: queue is full");
            g_cond_wait( me->fifo_cond, me->fifo_lock );
        }
        g_mutex_unlock( me->fifo_lock );
        GST_DEBUG("FIFO: queue is OK");
    }
    me->fifo[ me->fifo_tail ] = buf;
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
static GstBuffer*
fifo_pop(  Gstccnxsrc* me ) {
    GstBuffer* ans;
    int next;
        GST_DEBUG("FIFO: popping");
    if( fifo_empty (me) ) {
      return NULL;
    }
    next = me->fifo_head;
    ans = me->fifo[next];
    if( ++next >= CCNX_SRC_FIFO_MAX ) next = 0;
    g_mutex_lock( me->fifo_lock );
    me->fifo_head = next;
    g_cond_signal( me->fifo_cond );
    g_mutex_unlock( me->fifo_lock );
    return ans;
}

static GstTask	*eventTask;				/**< -> to a GST task structure */
static GMutex	*eventLock;				/**< -> a lock that helps control the task */
static GCond	*eventCond;				/**< -> a condition structure to help with synchronization */
static GStaticRecMutex	task_mutex = G_STATIC_REC_MUTEX_INIT; /**< I forget why we use this in this way */

/**
 * Base loop for the background CCN task
 *
 * This is the main execution loop for the background task responsible for
 * interacting with the CCN network. It is from this point that many of the above methods are
 * called to work the inbound messages from ccnx as well as sending new interest messages.
 *
 * \param data		the task context information setup by the parent sink element thread
 */
static void
ccn_event_thread(void *data) {
    Gstccnxsrc* src = (Gstccnxsrc*) data;
    struct ccn *ccn = src->ccn;
    int res = 0;

    GST_DEBUG ("*** event thread starting");
  /* We pass control to ccnx for a while so it can work with any incoming or outgoing data */
  /* We check to see if any problems have caused our ccnd connection to fail, and we reconnect */
    while (res >= 0) {
        res = ccn_run(ccn, 1000);
        if (res < 0 && ccn_get_connection_fd(ccn) == -1) {
            /* Try reconnecting, after a bit of delay */
            msleep((30 + (getpid() % 30)) * 1000);
            res = ccn_connect(ccn, ccndHost());
        }
    }
    GST_DEBUG ("*** event thread ending");
}

/**
 * Returns data to the pipeline for media processing
 *
 * Whe our downstream elements need more data, the GST framework sees to
 * it that this function is called so we can produce some data to give them.
 * For us that means taking data off of the FIFO being fed by the background
 * task. If it should be empty, we sit around and wait. Once data does
 * arrive, we take it and send it into the pipeline [we return].
 *
 * \param psrc		-> to the element context needing to produce data
 * \param offset	\todo I don't use this, why?
 * \param size		\todo I don't use this, why?
 * \param buf		where the data is to be placed
 * \return a GST status showing if we were successful in getting data
 * \retval GST_FLOW_OK buffer has been loaded with data
 * \retval GST_FLOW_ERROR something bad has happened
 */
static GstFlowReturn gst_ccnxsrc_create (GstBaseSrc * psrc, guint64 offset, guint size, GstBuffer ** buf)
{
  Gstccnxsrc *me;
  size_t sz;
  gboolean looping = TRUE;
  GstBuffer* ans = NULL;
  me = GST_CCNXSRC (psrc);
  GST_DEBUG ("create called");

  while( looping ) {
  GST_DEBUG ("create looping");
    if( fifo_empty(me) ) {
      msleep(50);
    } else {
      ans = fifo_pop(me);
      looping = FALSE;
    }
  }

  if( ans ) {
    sz = GST_BUFFER_SIZE(ans);
    GST_LOG_OBJECT (me, "got some data %d", sz);
    *buf = ans;
  } else {
    return GST_FLOW_ERROR;
  }
  GST_DEBUG ("create returning a buffer");

  return GST_FLOW_OK;
}

/**
 * Request the current segment number from the ccnx data producer
 *
 * This is a request for meta data for the media stream the user has named with their URI.
 * We assume that they want to join the media broadcast from what is currently being published
 * and \em not want to start from the beginning...although there is good reason for that too.
 *
 * Getting this data is done by expressing interests in the meta data. The producer of the
 * media stream will catch this through one of its ccnx filters and produce this meta data
 * back through the network to us.
 *
 * \param h		ccnx context handle
 * \param name	the content name for which we desire the meta data
 * \param timeout	how long to wait around
 * \return the segment number to ask for first, 0 on timeout
 */
long *
get_segment(struct ccn *h, struct ccn_charbuf *name, int timeout)
{
    struct ccn_charbuf *hn;
    long *result = NULL;
    int res;

  GST_INFO ("get_segment step 1");
    hn = ccn_charbuf_create();
    ccn_charbuf_append_charbuf(hn, name);
    ccn_name_append_str(hn, "_meta_");
    ccn_name_append_str(hn, ".segment");
  GST_INFO ("get_segment step 2");
    res = ccn_resolve_version(h, hn, CCN_V_HIGHEST, timeout);
  GST_INFO ("get_segment step 3, res: %d", res);
    if (res == 0) {
        struct ccn_charbuf *ho = ccn_charbuf_create();
        struct ccn_parsed_ContentObject pcobuf = { 0 };
        const unsigned char *hc;
        size_t hcs;

  GST_INFO ("get_segment step 10");
        res = ccn_get(h, hn, NULL, timeout, ho, &pcobuf, NULL, 0);
  GST_INFO ("get_segment step 11, res: %d", res);
        if (res == 0) {
            hc = ho->buf;
            hcs = ho->length;
            hDump( DUMP_ADDR(hc), DUMP_SIZE(hcs));
            ccn_content_get_value(hc, hcs, &pcobuf, &hc, &hcs);
            hDump( DUMP_ADDR(hc), DUMP_SIZE(hcs));
            result = calloc( 1, sizeof(long) );
            memcpy( result, hc, sizeof(long) );
        }
        ccn_charbuf_destroy(&ho);
    }
  GST_INFO ("get_segment step 9");
    ccn_charbuf_destroy(&hn);
    return (result);
}

/**
 * Tell the GST source element it is time to prepare to do work
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
 * \param bsrc		-> to the context source element
 * \return true if the initialization went well and we are ready to process data, false otherwise
 */
static gboolean
gst_ccnxsrc_start (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  struct ccn_charbuf *p_name = NULL;
  long *p_seg = NULL;
  int i_ret = 0;
  gboolean b_ret = FALSE;

  src = GST_CCNXSRC (bsrc);
  GST_DEBUG ("starting, getting connections");

  /* setup the connection to ccnx */
  if( (src->ccn = ccn_create()) == NULL ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("ccn_create failed"));
    return FALSE;
  }
  if( -1 == ccn_connect( src->ccn, ccndHost() ) ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("ccn_connect failed to %s", ccndHost()));
    return FALSE;
  }
  loadKey( src->ccn, &src->sp );

  /* A closure is what defines what to do when an inbound interest or data arrives */
  if( (src->ccn_closure = calloc(1, sizeof(struct ccn_closure))) == NULL ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("closure alloc failed"));
    return FALSE;
  }

  /* We setup the closure to keep our context [src] and to call the incoming_content() function */
  src->ccn_closure->data = src;
  src->ccn_closure->p = incoming_content;
  src->timeouts = 0;

  /* Allocate buffers and construct the name from the uri the user gave us */
  GST_INFO ("step 1");
  if( (p_name = ccn_charbuf_create()) == NULL ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("p_name alloc failed"));
    return FALSE;
  }
  if( (src->p_name = ccn_charbuf_create()) == NULL  ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("src->p_name alloc failed"));
    return FALSE;
  }
  if( (i_ret = ccn_name_from_uri(p_name, src->uri)) < 0 ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("name from uri failed for \"%s\"", src->uri));
    return FALSE;
  }

  /* Find out what the latest one of these is called, and keep it in our context */
  i_ret = ccn_resolve_version(src->ccn, p_name, CCN_V_HIGHEST,
                                CCN_VERSION_TIMEOUT);
  ccn_charbuf_append_charbuf(src->p_name, p_name);

  GST_INFO ("step 20 - name so far...");
  hDump(src->p_name->buf, src->p_name->length);
  src->i_seg = 0;
  if (i_ret == 0) { /* name is versioned, so get the meta data to obtain the length */
    p_seg = get_segment(src->ccn, p_name, CCN_HEADER_TIMEOUT);
    if (p_seg != NULL) {
        src->i_seg = *p_seg;
        GST_INFO("step 25 - next seg: %d", src->i_seg);
        free(p_seg);
    }
  }
  ccn_charbuf_destroy(&p_name);

  /* Even though the recent segment published is likely to be >> 0, we still need to ask for segment 0 */
  /* because it seems to contain valuable stream information. Attempts to skip segment 0 resulted in no */
  /* proper rendering of the stream on my screen during testing */
  p_name = sequenced_name(src->p_name, 0);
  
  GST_INFO ("step 30 - name for interest...");
  hDump(p_name->buf, p_name->length);
  i_ret = ccn_express_interest(src->ccn, p_name, src->ccn_closure,
                               src->p_template);
  ccn_charbuf_destroy(&p_name);
  if( i_ret < 0 ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("interest sending failed"));
    return FALSE;
  }

  GST_DEBUG ("interest sent for segment 0");

  /* Now start up the background work which will fetch all the rest of the R/T segments */
  eventTask = gst_task_create( ccn_event_thread, src );
  if( NULL == eventTask ) {
      GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("creating event thread failed"));
      return FALSE;
  }
  src->fifo_cond = g_cond_new();
  src->fifo_lock = g_mutex_new();
  gst_task_set_lock( eventTask, &task_mutex );
  eventCond = g_cond_new();
  eventLock = g_mutex_new();
  b_ret = gst_task_start(eventTask);
  if( FALSE == b_ret ) {
      GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("starting event thread failed"));
      return FALSE;
  }
  GST_DEBUG ("event thread started");

  /* Done! */
  return TRUE;
}

/**
 * Stop processing packets and relinquish resources
 *
 * This should do the complimentary actions to undo what start did.
 * Currently we do nothing.
 *
 * \param bsrc		element context that is being stopped
 * \return true if the stop worked properly, false otherwise
 */
static gboolean
gst_ccnxsrc_stop (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_DEBUG ("stopping, closing connections");

  return TRUE;
}

/**
 * Not sure what this is used for
 *
 * \todo find out what this is for some day
 *
 * \param bsrc		element context for the work we do
 * \return true if all went ok, false otherwise
 */
static gboolean
gst_ccnxsrc_unlock (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_LOG_OBJECT (src, "unlocking");

  return TRUE;
}

/**
 * Not sure what this is used for
 *
 * \todo find out what this is for some day
 *
 * \param bsrc		element context for the work we do
 * \return true if all went ok, false otherwise
 */
static gboolean
gst_ccnxsrc_unlock_stop (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_LOG_OBJECT (src, "No longer locked or whatever");

  return TRUE;
}

/**
 * Main working loop for stuff coming in from the CCNx network
 *
 * The only kind of content we work with are data messages. They are in response to the
 * interest messages we send out. The work involves 2 pieces: packing the data from ccn message
 * sizes into buffer sizes we use internally, and detecting when the stream of data is done.
 * 
 * The first is fairly simple. Each internal buffer we 'fill' is placed onto the fifo queue
 * so the main thread can take it off and reply to the pipeline request for more data.
 *
 * Determining the end of stream at the moment is a bit of a hack and could use some work.
 * \todo volunteers?  8-)
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
incoming_content(struct ccn_closure *selfp,
                 enum ccn_upcall_kind kind,
                 struct ccn_upcall_info *info)
{
    Gstccnxsrc *me = GST_CCNXSRC (selfp->data);

    const unsigned char *ccnb = NULL;
    size_t ccnb_size = 0;
    int64_t start_offset = 0;
    struct ccn_charbuf *name = NULL;
    const unsigned char *ib = NULL; /* info->interest_ccnb */
    struct ccn_indexbuf *ic = NULL;
    int i;
    int res;
    const unsigned char *data = NULL;
    size_t data_size = 0;
    gboolean b_last = FALSE;

  GST_INFO ("content has arrived!");
 
  /* Do some basic sanity and type checks to see if we want to process this data */
    switch (kind) {
    case CCN_UPCALL_FINAL:
        GST_LOG_OBJECT(me, "CCN upcall final %p", selfp);
        if (me->i_bufoffset > 0) {
            GST_BUFFER_SIZE(me->buf) = me->i_bufoffset;
            fifo_put(me, me->buf);
            me->buf = gst_buffer_new_and_alloc(CCN_FIFO_BLOCK_SIZE);
            me->i_bufoffset = 0;
        }
/*
 * Should emit an eos here instead of the empty buffer
 */
        GST_BUFFER_SIZE(me->buf) = 0;
        fifo_put(me, me->buf);
        me->i_bufoffset = 0;
        return (CCN_UPCALL_RESULT_OK);
    case CCN_UPCALL_INTEREST_TIMED_OUT:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN Interest timed out on dead closure %p", selfp);
            return(CCN_UPCALL_RESULT_OK);
        }
        GST_LOG_OBJECT(me, "CCN upcall reexpress -- timed out");
        if (me->timeouts > 5) {
            GST_LOG_OBJECT(me, "CCN upcall reexpress -- too many reexpressions");
            return(CCN_UPCALL_RESULT_OK);
        }
        me->timeouts++;
        return(CCN_UPCALL_RESULT_REEXPRESS);
    case CCN_UPCALL_CONTENT_UNVERIFIED:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN unverified content on dead closure %p", selfp);
            return(CCN_UPCALL_RESULT_OK);
        }
        return (CCN_UPCALL_RESULT_VERIFY);

    case CCN_UPCALL_CONTENT:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN content on dead closure %p", selfp);
            return(CCN_UPCALL_RESULT_OK);
        }
        break;
    default:
        GST_LOG_OBJECT(me, "CCN upcall result error");
        return(CCN_UPCALL_RESULT_ERR);
    }

	/* At this point it seems we have a data message we want to process */

    ccnb = info->content_ccnb;
    ccnb_size = info->pco->offset[CCN_PCO_E];

	/* spit out some debug information */
    for( i=0; i<5; ++i ) {
      const unsigned char *cp;
      size_t sz;
      GST_DEBUG ( "%3d: ", i);
      if( 0 > ccn_name_comp_get( info->content_ccnb, info->content_comps, i, &cp, &sz ) ) {
        fprintf(stderr, "could not get comp\n");
      } else
        hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }
    
	/* get the data from the ccn buffer into our buffer */
    ib = info->interest_ccnb;
    ic = info->interest_comps;
    res = ccn_content_get_value(ccnb, ccnb_size, info->pco, &data, &data_size);
    if (res < 0) {
        GST_LOG_OBJECT(me, "CCN error on get value of size");
        return(CCN_UPCALL_RESULT_ERR);
    }

    me->timeouts = 0;

    /* was this the last block? */
    /* \todo  the test below should get refactored into the library */
    if (info->pco->offset[CCN_PCO_B_FinalBlockID] !=
        info->pco->offset[CCN_PCO_E_FinalBlockID]) {
        const unsigned char *finalid = NULL;
        size_t finalid_size = 0;
        const unsigned char *nameid = NULL;
        size_t nameid_size = 0;
        struct ccn_indexbuf *cc = info->content_comps;
        ccn_ref_tagged_BLOB(CCN_DTAG_FinalBlockID, ccnb,
                            info->pco->offset[CCN_PCO_B_FinalBlockID],
                            info->pco->offset[CCN_PCO_E_FinalBlockID],
                            &finalid,
                            &finalid_size);
        if (cc->n < 2) abort();
        ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb,
                            cc->buf[cc->n - 2],
                            cc->buf[cc->n - 1],
                            &nameid,
                            &nameid_size);
        if (finalid_size == nameid_size && 0 == memcmp(finalid, nameid, nameid_size)) {
            b_last = TRUE;
        }
    }

    /* a short block can also indicate the end, if the client isn't using FinalBlockID */
    if (data_size < CCN_CHUNK_SIZE)
        b_last = TRUE;

    /* something to process */
    if (data_size > 0) {
  GST_DEBUG ("Have data!");
        start_offset = me->i_pos % CCN_CHUNK_SIZE;
        if (start_offset > data_size) {
            GST_LOG_OBJECT(me, "start_offset %zu > data_size %zu", start_offset, data_size);
        } else {
            if ((data_size - start_offset) + me->i_bufoffset > CCN_FIFO_BLOCK_SIZE) {
                /* won't fit in buffer, release the buffer upstream via the fifo queue */
  GST_DEBUG ("pushing data");
                GST_BUFFER_SIZE(me->buf) = me->i_bufoffset;
				fifo_put(me, me->buf);
                me->buf = gst_buffer_new_and_alloc(CCN_FIFO_BLOCK_SIZE);
                me->i_bufoffset = 0;
            }
            /* will fit in buffer */
            memcpy( GST_BUFFER_DATA(me->buf) + me->i_bufoffset, data + start_offset, data_size - start_offset);
            me->i_bufoffset += (data_size - start_offset);
        }
    }

    /* if we're done, indicate so with a 0-byte block, release any buffered data upstream,
     * and don't express an interest
     */
    if (b_last) {
  GST_DEBUG ("handling last block");
        if (me->i_bufoffset > 0) {
            GST_BUFFER_SIZE(me->buf) = me->i_bufoffset;
            fifo_put(me, me->buf);
            me->buf = gst_buffer_new_and_alloc(CCN_FIFO_BLOCK_SIZE);
            me->i_bufoffset = 0;
        }
/*
 * \todo should emit an eos here instead of the empty buffer
 */
        GST_BUFFER_SIZE(me->buf) = 0;
        fifo_put(me, me->buf);
        me->i_bufoffset = 0;
        return (CCN_UPCALL_RESULT_OK);
    }

	/* Dropping through to here means it was not the last block */
    /* Ask for the next segment from the producer */
    me->i_pos = CCN_CHUNK_SIZE * (1 + (me->i_pos / CCN_CHUNK_SIZE));
    name = sequenced_name(me->p_name, me->i_seg++ );
    res = ccn_express_interest(info->h, name, selfp, NULL);
    ccn_charbuf_destroy(&name);
  GST_DEBUG ("new interest sent");

    if (res < 0) {
        GST_LOG_OBJECT(me, "trouble sending the next interests");
        return (CCN_UPCALL_RESULT_ERR);
    }

    return(CCN_UPCALL_RESULT_OK);

}


/*** GSTURIHANDLER INTERFACE *************************************************/

/**
 * Retrieves the type of URI processing element we are
 *
 * \return the type of URI
 * \retval GST_URI_SRC	the one and only thing returned
 */
static GstURIType
gst_ccnxsrc_uri_get_type (void)
{
  return GST_URI_SRC;
}

/**
 * Retrieves the protocol type we accept in the URI
 *
 * \return an array of character array pointers, one for each recognized protocol
 * \retval ccnx	this is the only recognized protocol at this time
 */
static gchar **
gst_ccnxsrc_uri_get_protocols (void)
{
  static gchar *protocols[] = { "ccnx", NULL };

  return protocols;
}

/**
 * Retrieve the URI the user specified
 *
 * \param handler		element context that has the URI information
 * \return character array of the URI
 */
static const gchar *
gst_ccnxsrc_uri_get_uri (GstURIHandler * handler)
{
  Gstccnxsrc *me = GST_CCNXSRC (handler);

  return me->uri;
}

/**
 * Set the URI the user specified
 *
 * \param handler		element context that has the URI information
 * \param uri			the URI to set in the element context
 * \return true if it worked ok, false otherwise
 */
static gboolean
gst_ccnxsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  gboolean ret;

  Gstccnxsrc *me = GST_CCNXSRC (handler);

  ret = gst_ccnxsrc_set_uri (me, uri);

  return ret;
}

/**
 * Initializer for the URI handler capabilities of this element
 *
 * Sets up the function call table for dealing with URIs.
 *
 * \param g_iface		\todo not sure
 * \param iface_data	\todo not sure
 */
static void
gst_ccnxsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_ccnxsrc_uri_get_type;
  iface->get_protocols = gst_ccnxsrc_uri_get_protocols;
  iface->get_uri = gst_ccnxsrc_uri_get_uri;
  iface->set_uri = gst_ccnxsrc_uri_set_uri;
}

/**
 * \date Created: Nov, 2009
 * \author John Letourneau <topgun@bell-labs.com>
 *
 * \page CCNSRCDESIGN CCNx GStreamer Source Design Unit
 *
 * This is primarily implemented in the ccnsrc.c file;
 * others using these capabilities should include the ccnxsrc.h file.
 * We also make use of functionality implemented in utils.c [utils.h].
 *
 * This GStreamer element is responsible for generating interests in a media stream of data,
 * and consuming packets resulting from those interests. The data flows over
 * a CCNx network and data is placed onto a GST pipeline
 * for consumption by other GST elements.
 * \code
                <--- interests |-----------------| FIFO |---------|--------|---------|---------|
    CCNx Network ---------->   | background task | ---> | ccnsrc  | elem-1 | elem-2  |  . . .  |
                      Data     |                 |      |         |        |         |         |
                               |-----------------|      |---------|--------|---------|---------|
 * \endcode
 * As this element operates, its background task will express interests onto the CCN network.
 * These interest are fully qualified names which means they include the segment number.
 * A window of outstanding interests is maintained.
 * As data arrives to satisfy these interests, additional interests are generated to
 * keep the window of outstanding work open.
 *
 * These data messages and then re-packaged into buffers of different size than the CCN message size.
 * These are then passed along to the main source element code which sees to it that they
 * are passed into the pipeline when asked for.
 * Details of how the FIFO queue works can be found at \ref SINKFIFOQUEUE.
 * For a discussion of content naming, please see the complement information in \subpage CCNSINKDESIGN.
 * Specifically see section \ref SINKCCNNAMING.
 */