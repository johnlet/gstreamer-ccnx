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

/**
 * SECTION:element-ccnxsrc
 *
 * FIXME:Describe ccnxsrc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m ccnxsrc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
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

GST_DEBUG_CATEGORY_STATIC (gst_ccnxsrc_debug);
#define GST_CAT_DEFAULT gst_ccnxsrc_debug

#define CCN_FIFO_MAX_BLOCKS 128
#define CCN_CHUNK_SIZE 4000
#define CCN_FIFO_BLOCK_SIZE (1 * CCN_CHUNK_SIZE)
#define CCN_VERSION_TIMEOUT 400
#define CCN_HEADER_TIMEOUT 400


/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0
  , PROP_URI
  , PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static const GstElementDetails gst_ccnxsrc_details = GST_ELEMENT_DETAILS(
	"CCNX interest source",
	"Source/Network",
	"Receive data over a CCNx network via interests being sent out",
	"John Letourneau <john.letourneau@alcatel-lucent.com" );

#define CCNX_NETWORK_DEFAULT_PORT		1111
#define CCNX_NETWORK_DEFAULT_ADDR		1.2.3.4
#define CCNX_DEFAULT_URI			"ccnx://error"

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

GST_BOILERPLATE_FULL (Gstccnxsrc, gst_ccnxsrc, GstPushSrc,
    GST_TYPE_PUSH_SRC, _do_init);

/* GObject vmethod implementations */

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

/* initialize the ccnxsrc's class */
static void
gst_ccnxsrc_class_init (GstccnxsrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  
	GST_DEBUG("CCNxSrc: class init");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;

  gobject_class->set_property = gst_ccnxsrc_set_property;
  gobject_class->get_property = gst_ccnxsrc_get_property;
  gobject_class->finalize = gst_ccnxsrc_finalize;

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI of the form: ccnx://<content name>",
          CCNX_DEFAULT_URI, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gstbasesrc_class->start = gst_ccnxsrc_start;
  gstbasesrc_class->stop = gst_ccnxsrc_stop;
  gstbasesrc_class->unlock = gst_ccnxsrc_unlock;
  gstbasesrc_class->unlock_stop = gst_ccnxsrc_unlock_stop;
  gstbasesrc_class->get_caps = gst_ccnxsrc_getcaps;
  gstbasesrc_class->create = gst_ccnxsrc_create;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
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

static gboolean
gst_ccnxsrc_set_uri (Gstccnxsrc * src, const gchar * uri)
{
  gchar *protocol;
  gchar *location;
  gchar *colptr;

  protocol = gst_uri_get_protocol (uri);
  if (strcmp (protocol, "ccnx") != 0)
    goto wrong_protocol;
  g_free (protocol);

  location = gst_uri_get_location (uri);
  if (!location)
    return FALSE;
  colptr = strrchr (location, ':');
  g_free (location);

  src->uri = g_strdup(uri);

  return TRUE;

  /* ERRORS */
wrong_protocol:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("error parsing uri %s: wrong protocol (%s != ccnx)", uri, protocol));
    g_free (protocol);
    return FALSE;
  }
}

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

static void
gst_ccnxsrc_finalize(GObject *obj) {

  Gstccnxsrc *me;

  me = GST_CCNXSRC (obj);

  if (me->caps)
    gst_caps_unref (me->caps);
  g_free (me->uri);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
msleep( int msecs ) {
  struct timespec tv, rm;
  tv.tv_sec = 0;
  while( msecs > 999 ) {
    tv.tv_sec++;
    msecs -= 1000;
  }
  tv.tv_nsec = msecs * 1000000;
  nanosleep( &tv, &rm );
}


static struct ccn_charbuf *
sequenced_name(struct ccn_charbuf *basename, uintmax_t seq)
{
    struct ccn_charbuf *name = NULL;

    name = ccn_charbuf_create();
    ccn_charbuf_append_charbuf(name, basename);
    ccn_name_append_numeric(name, CCN_MARKER_SEQNUM, seq);
    return(name);
}

/* V-I
 * The fifo is primarily a no-lock queue...however there is special treatment to deal with a queue full condition.
 * The put code will only use a lock when the queue is found to be full. Then it performs the is-empty test under the
 * control of a lock, waiting on a condition to be signaled if it is still found to be full.
 * The pop code will modify its fifo index under the control of this same lock. It then always signals the condition
 * for which the put code might be waiting for.
 * This could be optimized by only sending the condition if the pop code finds it is going from a full to a non-full fifo.
 */

static gboolean
fifo_empty( Gstccnxsrc* me ) {
    return me->fifo_head == me->fifo_tail;
}

static gboolean
fifo_put(  Gstccnxsrc* me, GstBuffer *buf ) {
    int next;
        GST_DEBUG("FIFO: putting");
    next = me->fifo_tail;
    if( ++next >= CCNX_FIFO_MAX ) next = 0;
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
    if( ++next >= CCNX_FIFO_MAX ) next = 0;
    g_mutex_lock( me->fifo_lock );
    me->fifo_head = next;
    g_cond_signal( me->fifo_cond );
    g_mutex_unlock( me->fifo_lock );
    return ans;
}

static GstTask	*eventTask;
static GMutex	*eventLock;
static GCond	*eventCond;
static GStaticRecMutex	task_mutex = G_STATIC_REC_MUTEX_INIT;

static void
ccn_event_thread(void *data) {
    Gstccnxsrc* src = (Gstccnxsrc*) data;
    struct ccn *ccn = src->ccn;
    int res = 0;

    GST_DEBUG ("*** event thread starting");
    while (res >= 0) {
        res = ccn_run(ccn, 1000);
    GST_DEBUG ("*** et: loop");
        if (res < 0 && ccn_get_connection_fd(ccn) == -1) {
            /* Try reconnecting, after a bit of delay */
            msleep((30 + (getpid() % 30)) * 1000);
            res = ccn_connect(ccn, ccndHost());
        }
    }
    GST_DEBUG ("*** event thread ending");
}


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


/* create a connection for getting data */
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

  if( (src->ccn = ccn_create()) == NULL ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("ccn_create failed"));
    return FALSE;
  }
  if( -1 == ccn_connect( src->ccn, ccndHost() ) ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("ccn_connect failed to %s", ccndHost()));
    return FALSE;
  }
  loadKey( src->ccn, &src->sp );

  if( (src->ccn_closure = calloc(1, sizeof(struct ccn_closure))) == NULL ) {
    GST_ELEMENT_ERROR( src, RESOURCE, READ, (NULL), ("closure alloc failed"));
    return FALSE;
  }

  src->ccn_closure->data = src;
  src->ccn_closure->p = incoming_content;
  src->i_size = LONG_MAX;
  src->timeouts = 0;
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
  i_ret = ccn_resolve_version(src->ccn, p_name, CCN_V_HIGHEST,
                                CCN_VERSION_TIMEOUT);
  ccn_charbuf_append_charbuf(src->p_name, p_name);

  GST_INFO ("step 20 - name so far...");
  hDump(src->p_name->buf, src->p_name->length);
  src->i_seg = 0;
  if (i_ret == 0) { /* name is versioned, so get the header to obtain the length */
    p_seg = get_segment(src->ccn, p_name, CCN_HEADER_TIMEOUT);
    if (p_seg != NULL) {
        src->i_seg = *p_seg;
        GST_INFO("step 25 - next seg: %d", src->i_seg);
        free(p_seg);
    }
  }
  ccn_charbuf_destroy(&p_name);
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

  GST_DEBUG ("interest sent");

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

  return TRUE;
}


static gboolean
gst_ccnxsrc_stop (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_DEBUG ("stopping, closing connections");

  return TRUE;
}


static gboolean
gst_ccnxsrc_unlock (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_LOG_OBJECT (src, "unlocking");

  return TRUE;
}

static gboolean
gst_ccnxsrc_unlock_stop (GstBaseSrc * bsrc)
{
  Gstccnxsrc *src;

  src = GST_CCNXSRC (bsrc);

  GST_LOG_OBJECT (src, "No longer locked or whatever");

  return TRUE;
}

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


    ccnb = info->content_ccnb;
    ccnb_size = info->pco->offset[CCN_PCO_E];
    for( i=0; i<5; ++i ) {
      const unsigned char *cp;
      size_t sz;
      GST_DEBUG ( "%3d: ", i);
      if( 0 > ccn_name_comp_get( info->content_ccnb, info->content_comps, i, &cp, &sz ) ) {
        fprintf(stderr, "could not get comp\n");
      } else
        hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }
    
    ib = info->interest_ccnb;
    ic = info->interest_comps;
    res = ccn_content_get_value(ccnb, ccnb_size, info->pco, &data, &data_size);
    if (res < 0) {
        GST_LOG_OBJECT(me, "CCN error on get value of size");
        return(CCN_UPCALL_RESULT_ERR);
    }

    me->timeouts = 0;

    /* was this the last block? */
    /* TODO:  the test below should get refactored into the library */
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
                /* won't fit in buffer, release the buffer upstream */
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
 * Should emit an eos here instead of the empty buffer
 */
        GST_BUFFER_SIZE(me->buf) = 0;
        fifo_put(me, me->buf);
        me->i_bufoffset = 0;
        return (CCN_UPCALL_RESULT_OK);
    }



    /* Ask for the next fragment */
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

static GstURIType
gst_ccnxsrc_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_ccnxsrc_uri_get_protocols (void)
{
  static gchar *protocols[] = { "ccnx", NULL };

  return protocols;
}

static const gchar *
gst_ccnxsrc_uri_get_uri (GstURIHandler * handler)
{
  Gstccnxsrc *src = GST_CCNXSRC (handler);

  return src->uri;
}

static gboolean
gst_ccnxsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  gboolean ret;

  Gstccnxsrc *src = GST_CCNXSRC (handler);

  ret = gst_ccnxsrc_set_uri (src, uri);

  return ret;
}

static void
gst_ccnxsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_ccnxsrc_uri_get_type;
  iface->get_protocols = gst_ccnxsrc_uri_get_protocols;
  iface->get_uri = gst_ccnxsrc_uri_get_uri;
  iface->set_uri = gst_ccnxsrc_uri_set_uri;
}
