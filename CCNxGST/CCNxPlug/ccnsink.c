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

/**
 * SECTION:element-ccnxsink
 *
 * FIXME:Describe ccnxsink here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ccnxsink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/netbuffer/gstnetbuffer.h>
#include <glib/gstdio.h>

#include "ccnxsink.h"
#include <ccn/keystore.h>
#include <ccn/signing.h>

#include <limits.h>
#include <stdlib.h>
#include "utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_ccnxsink_debug);
#define GST_CAT_DEFAULT gst_ccnxsink_debug

#define CCN_FIFO_MAX_BLOCKS 128
#define CCN_CHUNK_SIZE 4000
#define CCN_FIFO_BLOCK_SIZE (128 * CCN_CHUNK_SIZE)
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
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static const GstElementDetails gst_ccnxsink_details = GST_ELEMENT_DETAILS(
	"CCNX data sink",
	"Source/Network",
	"Publish data over a CCNx network",
	"John Letourneau <topgun@bell-labs.com" );

#define CCNX_NETWORK_DEFAULT_PORT		1111
#define CCNX_NETWORK_DEFAULT_ADDR		1.2.3.4
#define CCNX_DEFAULT_URI			"ccnx:/error"
#define CCNX_DEFAULT_EXPIRATION     -1
static struct ccn_signing_params CCNX_DEFAULT_SIGNING_PARAMS = CCN_SIGNING_PARAMS_INIT;

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
static GstCaps * gst_ccnxsink_getcaps (GstBaseSink * src);

static GstFlowReturn gst_ccnxsink_publish (GstBaseSink * sink, GstBuffer * buf);


static void
_do_init (GType type)
{

}

GST_BOILERPLATE_FULL (Gstccnxsink, gst_ccnxsink, GstBaseSink,
    GST_TYPE_BASE_SINK, _do_init);

static void
gst_ccnxsink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);
  
  GST_DEBUG_CATEGORY_INIT (gst_ccnxsink_debug, "ccnxsink", 0, "CCNx sink");

  gst_element_class_set_details(element_class,
     &gst_ccnxsink_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));
}

/* initialize the ccnxsrc's class */
static void
gst_ccnxsink_class_init (GstccnxsinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;
  
	GST_DEBUG("CCNxSink: class init");

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_ccnxsink_set_property;
  gobject_class->get_property = gst_ccnxsink_get_property;

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI of the form: ccnx://<content name>",
          CCNX_DEFAULT_URI, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gobject_class->finalize = gst_ccnxsink_finalize;
  gstbasesink_class->start = gst_ccnxsink_start;
  gstbasesink_class->stop = gst_ccnxsink_stop;
  gstbasesink_class->get_times = NULL;
  gstbasesink_class->get_caps = gst_ccnxsink_getcaps;
  gstbasesink_class->render = gst_ccnxsink_publish;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_ccnxsink_init (Gstccnxsink * me,
    GstccnxsinkClass * gclass)
{
  /*
  me->sinkpad = gst_pad_new_from_static_template (&ssinktemplate, "sink");
  gst_pad_set_getcaps_function (me->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (me), me->sinkpad);
  */
	GST_DEBUG("CCNxSink: instance init");

  gst_base_sink_set_sync (GST_BASE_SINK (me), FALSE);

  me->silent = FALSE;
  me->uri = g_strdup(CCNX_DEFAULT_URI);
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
  memcpy( &(me->sp), &CCNX_DEFAULT_SIGNING_PARAMS, sizeof(CCNX_DEFAULT_SIGNING_PARAMS) );
  me->expire = CCNX_DEFAULT_EXPIRATION;
  me->segment = 0;
  me->fifo_head = 0;
  me->fifo_tail = 0;
  me->i_pos = 0;
  me->i_bufoffset = 0;
  me->o_bufoffset = 0;
  me->buf = gst_buffer_new_and_alloc(CCN_FIFO_BLOCK_SIZE);
  me->obuf = NULL;

}

static GstCaps *
gst_ccnxsink_getcaps (GstBaseSink * src)
{
  Gstccnxsink *me;
	GST_DEBUG("CCNxSink: get caps");

  me = GST_CCNXSINK (src);

  if (me->caps)
    return gst_caps_ref (me->caps);
  else
    return gst_caps_new_any ();
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

/* V-II
 * The fifo is primarily a no-lock queue...however there is special treatment to deal with a queue full condition.
 * The put code will only use a lock when the queue is found to be full.
 * If the overwrite flag is true, the head/tail are adjusted in a way that the oldest [next to pop] item is discarded.
 * This has the effect of keeping the data producer moving at a cost of losing some data.
 * If the overwrite is false, then the producer will wait for space. The code performs the is-empty test under the
 * control of a lock, waiting on a condition to be signaled if it is still found to be full.
 * The pop code will modify its fifo index under the control of this same lock. It then always signals the condition
 * for which the put code might be waiting for.
 * This could be optimized by only sending the condition if the pop code finds it is going from a full to a non-full fifo.
 */

static gboolean
fifo_empty( Gstccnxsink* me ) {
    return me->fifo_head == me->fifo_tail;
}

static gboolean
fifo_put(  Gstccnxsink* me, GstBuffer *buf, int overwrite ) {
    int next;
        GST_DEBUG("CCNxSink: fifo putting");
    next = me->fifo_tail;
    if( ++next >= CCNX_FIFO_MAX ) next = 0;
    if( next  == me->fifo_head ) {
        g_mutex_lock( me->fifo_lock );
        if( overwrite ) {
          if( next == me->fifo_head ) { /* repeat test under a lock */
            int h = me->fifo_head;
            if( ++h >= CCNX_FIFO_MAX ) h = 0; /* throw the head one away */
			/* need to release the buffer prior to the overwrite, or memory will leak */
			gst_buffer_unref( me->fifo[ me->fifo_head ] );
            me->fifo_head = h;
            GST_LOG_OBJECT(me, "fifo put: overwriting a buffer");
          }
        } else {
	        while( next  == me->fifo_head ) {
	            GST_DEBUG("FIFO: queue is full");
	            g_cond_wait( me->fifo_cond, me->fifo_lock );
	        }
        }
        g_mutex_unlock( me->fifo_lock );
        GST_DEBUG("FIFO: queue is OK");
    }
    me->fifo[ me->fifo_tail ] = buf;
    me->fifo_tail = next;
    return TRUE;
}

static GstBuffer*
fifo_pop(  Gstccnxsink* me ) {
    GstBuffer* ans;
    int next;
	GST_DEBUG("CCNxSink: fifo popping");
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

#include <time.h>

#define A_THOUSAND    1000
#define A_MILLION     (1000*A_THOUSAND)
#define A_BILLION     (1000*A_MILLION)

GstClockTime
tNow() {
  struct timespec t;
  
  clock_gettime( CLOCK_REALTIME, &t );
  return (t.tv_sec * A_BILLION + t.tv_nsec);
}

void
makeTimespec( GstClockTime t, struct timespec * ts ) {
  if( NULL == ts ) return;
  ts->tv_sec = t / A_BILLION;
  ts->tv_nsec = t % A_BILLION;
}


static GstFlowReturn
gst_ccnxsink_send ( Gstccnxsink *me, guint8 *data, guint size, GstClockTime ts ) {
  struct ccn_charbuf  *sname;
  struct ccn_charbuf  *hold;
  struct ccn_charbuf  *temp;
  struct ccn_charbuf  *signed_info;
  gint                rc;
  guint8              *xferStart;
  guint               bytesLeft;

  xferStart = data;
  bytesLeft = size;
  sname = ccn_charbuf_create();
  temp = ccn_charbuf_create();
  signed_info = ccn_charbuf_create();
  hold = me->lastPublish;
  
/*
  if( me->keystore ) {
    signed_info->length = 0;
GST_LOG_OBJECT( me, "send - signing info\n" );
    rc = ccn_signed_info_create(signed_info,
                                 /*pubkeyidccn_keystore_public_key_digest(me->keystore),
                                 /*publisher_key_id_sizeccn_keystore_public_key_digest_length(me->keystore),
                                 /*datetimeNULL,
                                 /*typeCCN_CONTENT_DATA,
                                 /*freshness me->expire,
                                 /*finalblockidNULL,
                                 me->keylocator);
    /* Put the keylocator in the first block only. 
    ccn_charbuf_destroy(&(me->keylocator));
    if (rc < 0) {
        GST_LOG_OBJECT(me, "Failed to create signed_info (rc == %d)\n", rc);
        goto Trouble;
    }
  }
	*/

  if( me->partial ) { /* We had some left over from the last send */
    guint extra;
    extra = CCN_CHUNK_SIZE - me->partial->length;
    if( extra > bytesLeft ) extra = bytesLeft;
    GST_LOG_OBJECT( me, "send - had a partial left: %d\n", extra );
    ccn_charbuf_append( me->partial, xferStart, extra );
    bytesLeft -= extra;
    xferStart += extra;
    
    if( me->partial->length == CCN_CHUNK_SIZE ) {
      sname->length = 0;
      ccn_charbuf_append(sname, me->name->buf, me->name->length);
      ccn_name_append_numeric(sname, CCN_MARKER_SEQNUM, me->segment++);
      temp->length = 0;

	  ccn_sign_content(me->ccn, temp, sname, &me->sp, me->partial->buf, CCN_CHUNK_SIZE);
  /*
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
		rc = ccn_put(me->ccn, temp->buf, temp->length);
		if (rc < 0) {
          GST_LOG_OBJECT(me, "ccn_put failed (rc == %d)\n", rc);
          goto Trouble;
		}
      
		ccn_charbuf_destroy(&me->partial);
		me->partial = NULL;
/*
} else {
		GST_LOG_OBJECT( me, "No keystore. What should we do?\n" );
		goto Trouble;
	  }
*/
	}
  } /* No left over means we can send direct out of the data buffer */

  while( bytesLeft >= CCN_CHUNK_SIZE ) {
    GST_LOG_OBJECT( me, "send - bytesLeft: %d\n", bytesLeft );
    sname->length = 0;
    ccn_charbuf_append(sname, me->name->buf, me->name->length);
    ccn_name_append_numeric(sname, CCN_MARKER_SEQNUM, me->segment++);
    GST_LOG_OBJECT( me, "send - name is ready\n" );
    temp->length = 0;

	  ccn_sign_content(me->ccn, temp, sname, &me->sp, xferStart, CCN_CHUNK_SIZE);
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
    GST_LOG_OBJECT( me, "send - putting\n" );
		rc = ccn_put(me->ccn, temp->buf, temp->length);
		if (rc < 0) {
          GST_LOG_OBJECT(me, "ccn_put failed (rc == %d)\n", rc);
          goto Trouble;
		}
      /*
	  } else {
		GST_LOG_OBJECT( me, "No keystore. What should we do?\n" );
		goto Trouble;
	  }
*/
    GST_LOG_OBJECT( me, "send - adjusting buffers\n" );
	/* msleep(5); */
    bytesLeft -= CCN_CHUNK_SIZE;
    xferStart += CCN_CHUNK_SIZE;
  }
  
  if( bytesLeft ) { /* We have some left over for next time */
    GST_LOG_OBJECT( me, "send - for next time: %d\n", bytesLeft );
    me->partial = ccn_charbuf_create();
    me->partial->length = 0;
    ccn_charbuf_append( me->partial, xferStart, bytesLeft );
  }

	ccn_charbuf_destroy(&sname);
	me->lastPublish = temp;
	ccn_charbuf_destroy(&hold);
	ccn_charbuf_destroy(&signed_info);
	GST_LOG_OBJECT( me, "send - leaving length: %d\n", me->lastPublish->length );
	return CCN_UPCALL_RESULT_OK;

Trouble:
	ccn_charbuf_destroy(&sname);
	ccn_charbuf_destroy(&temp);
	ccn_charbuf_destroy(&signed_info);
	return CCN_UPCALL_RESULT_ERR;
}

static GstFlowReturn
gst_ccnxsink_publish (GstBaseSink * sink, GstBuffer * buffer)
{
  Gstccnxsink *me;
  GstClockTime ts;
  struct ccn_charbuf *sname;
  guint size;
  guint8 *data;
  int rc;

	GST_DEBUG("CCNxSink: publishing");
  me = GST_CCNXSINK (sink);
	if(1) {
		gst_buffer_ref( buffer );
		fifo_put(me, buffer, TRUE);
		return GST_FLOW_OK;
	}

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);
/*  ts = GST_BUFFER_TIMESTAMP(buffer); */
  ts = 0;
  
    GST_LOG_OBJECT( me, "pub - enter length[%08X]: %d\n", (int)me, me->lastPublish->length );
  
	GST_INFO("CCNxSink: pubish size: %d\n", size);
	if( 0 == ts || GST_CLOCK_TIME_NONE == ts )
	  ts = me->ts;
	if( 0 == ts || GST_CLOCK_TIME_NONE == ts ) {
	  ts = tNow();
	  me->ts = ts;
	}

	GST_INFO("CCNxSink: pubish time: %0X\n", ts);
  if(1) return gst_ccnxsink_send( me, data, size, ts );
  
  return GST_FLOW_OK;
}

/*
 * We get notified that a client has interests in things we are producing.
 * For now we just send out the last thing we produced. Will need to 'buffer' this
 * a bit more, especially for a stream like audio/video.
 * I need to see how the interests change with time, and also will need to
 * name the content appropriately. Then I'll create an LRU loop of data as it is
 * created...which in this case means we get it on a sink pad in our render method: gst_ccnxsink_publish.
 */
static enum ccn_upcall_res
new_interests(struct ccn_closure *selfp,
                 enum ccn_upcall_kind kind,
                 struct ccn_upcall_info *info)
{
    Gstccnxsink *me = GST_CCNXSINK (selfp->data);
    struct ccn_charbuf* cb;
	struct ccn_charbuf* sname = NULL;
    const char *cp1, *cp2;
    int sz1;
	size_t sz2;
    long lastSeq;
    struct ccn_signing_params myparams = CCN_SIGNING_PARAMS_INIT;
    int i;
    int rc;


  GST_DEBUG ("something has arrived!");
  GST_DEBUG ("matched is: %d", info->matched_comps);
  cb = interestAsUri(info);
  GST_DEBUG ("as URI: %s", ccn_charbuf_as_string( cb ));
  ccn_charbuf_destroy(&cb);
    for( i=0; i<10; ++i ) {
      const unsigned char *cp;
      size_t sz;
      GST_DEBUG ( "%3d: ", i);
      if( 0 > ccn_name_comp_get( info->interest_ccnb, info->interest_comps, i, &cp, &sz ) ) {
        fprintf(stderr, "could not get comp\n");
        break;
      } else
        hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }
 
    switch (kind) {

    case CCN_UPCALL_FINAL:
        GST_LOG_OBJECT(me, "CCN upcall final %p", selfp);
        return (0);

    case CCN_UPCALL_INTEREST_TIMED_OUT:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN Interest timed out on dead closure %p", selfp);
            return(0);
        }
        GST_LOG_OBJECT(me, "CCN upcall reexpress -- timed out");
        if (me->timeouts > 5) {
            GST_LOG_OBJECT(me, "CCN upcall reexpress -- too many reexpressions");
            return(0);
        }
        me->timeouts++;
        return(CCN_UPCALL_RESULT_REEXPRESS);

    case CCN_UPCALL_CONTENT_UNVERIFIED:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN unverified content on dead closure %p", selfp);
            return(0);
        }
        return (CCN_UPCALL_RESULT_VERIFY);

    case CCN_UPCALL_CONTENT:
        if (selfp != me->ccn_closure) {
            GST_LOG_OBJECT(me, "CCN content on dead closure %p", selfp);
            return(0);
        }
        break;

    case CCN_UPCALL_CONTENT_BAD:
	GST_LOG_OBJECT(me, "Content signature verification failed! Discarding.\n");
	return (CCN_UPCALL_RESULT_ERR);

    case CCN_UPCALL_CONSUMED_INTEREST:
        GST_LOG_OBJECT(me, "Upcall consumed interest\n");
        return (CCN_UPCALL_RESULT_ERR); /* no data */

    case CCN_UPCALL_INTEREST:
        GST_INFO("We got an interest\n");
        myparams.freshness = 1; /* meta data is old very quickly */

        if( 0 <= ccn_name_comp_get( info->interest_ccnb, info->interest_comps, 3, (const unsigned char**)&cp1, &sz1 ) &&
             0 <= ccn_name_comp_get( info->interest_ccnb, info->interest_comps, 4, (const unsigned char**)&cp2, &sz2 ) ) {
          hDump( DUMP_ADDR(cp1), DUMP_SIZE(sz1) );
          hDump( DUMP_ADDR(cp2), DUMP_SIZE(sz2) );
          if( strncmp( cp1, "_meta_", 6 ) || strncmp( cp2, ".segment", 8 ) ) goto Exit_Interest; /* not a match */
          /* publish what segment we are up to in reply to the meta request */
          lastSeq = me->segment - 1;
          GST_INFO("sending meta data...1...segment: %d", lastSeq);
          
          sname = ccn_charbuf_create();
          ccn_name_init(sname);
          GST_INFO("sending meta data...2");
          rc = ccn_name_append_components(sname, info->interest_ccnb, info->interest_comps->buf[0], info->interest_comps->buf[info->interest_comps->n-1]);
          GST_INFO("sending meta data...3");
          if (rc < 0) goto Error_Interest;
          rc = ccn_create_version(me->ccn, sname, CCN_V_REPLACE | CCN_V_NOW | CCN_V_HIGH, 0, 0);
          GST_INFO("sending meta data...3.5");
          if (rc < 0) goto Error_Interest;
          me->temp->length=0;
          GST_INFO("sending meta data...4");
          rc = ccn_sign_content(me->ccn, me->temp, sname, &myparams,
                          &lastSeq, sizeof(lastSeq));
          GST_INFO("sending meta data...5");
          hDump(DUMP_ADDR(sname->buf), DUMP_SIZE(sname->length));
          if (rc != 0) {
              GST_LOG_OBJECT(me, "Failed to encode ContentObject (rc == %d)\n", rc);
              goto Error_Interest;
          }
          
          rc = ccn_put(me->ccn, me->temp->buf, me->temp->length);
          me->temp->length = 0;
          if (rc < 0) {
              GST_LOG_OBJECT(me, "ccn_put failed (res == %d)\n", rc);
              goto Error_Interest;
          }
          GST_INFO("meta data sent");
        } else goto Exit_Interest; /* do not have _meta_ / .segment */

        
        if(0) fifo_pop(0); /* keep compiler quiet for now */
        
Exit_Interest:
        ccn_charbuf_destroy(&sname);
        break;
        
Error_Interest:
        ccn_charbuf_destroy(&sname);
        return CCN_UPCALL_RESULT_ERR;


    default:
        GST_LOG_OBJECT(me, "CCN upcall result error");
        return(CCN_UPCALL_RESULT_ERR);
    }


    me->timeouts = 0;


    return(CCN_UPCALL_RESULT_OK);

}

static void
setup_ccn(Gstccnxsink* me) {
  struct ccn *ccn;
  
  GST_DEBUG ("CCNxSink: setup name...");
  if( (me->name = ccn_charbuf_create()) == NULL ) {
    GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("name alloc failed"));
    return;
  }
  if( ccn_name_from_uri(me->name, me->uri) < 0 ) {
    GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("name from uri failed"));
    return;
  }

  GST_DEBUG ("CCNxSink: creating ccn object");
  if( (ccn = ccn_create()) == NULL ) {
    GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("ccn_create failed"));
    return;
  }
  me->ccn = ccn;
  GST_DEBUG ("CCNxSink: connecting");
  if( -1 == ccn_connect( me->ccn, ccndHost() ) ) {
    GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("ccn_connect failed to %s", ccndHost()));
    return;
  }

  GST_DEBUG ("CCNxSink: setting name version");
  if (0 > ccn_create_version(ccn, me->name, CCN_V_REPLACE | CCN_V_NOW | CCN_V_HIGH, 0, 0)) {
      GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("ccn_create_version() failed"));
      return;
  }


  GST_DEBUG ("CCNxSink: setting up keystore");
  /*
  me->keystore = fetchStore();
  if( me->keystore ) me->keylocator = makeLocator( ccn_keystore_public_key(me->keystore) );
  */
  loadKey( me->ccn, &me->sp );
  GST_DEBUG ("CCNxSink: done; have keys!");
}

static void
check_fifo(Gstccnxsink* me) {
  GstClockTime ts;
  gint i;
  guint size;
  guint8 *data;
  GstBuffer *buffer;

  for( i=0; i<3; ++i ) {
	if( fifo_empty(me) ) return;
	buffer = fifo_pop(me);
	size = GST_BUFFER_SIZE (buffer);
	data = GST_BUFFER_DATA (buffer);
	ts = 0;
  
	GST_INFO("CCNxSink: pubish size: %d\n", size);
	if( 0 == ts || GST_CLOCK_TIME_NONE == ts )
	  ts = me->ts;
	if( 0 == ts || GST_CLOCK_TIME_NONE == ts ) {
	  ts = tNow();
	  me->ts = ts;
	}

	GST_INFO("CCNxSink: pubish time: %0X\n", ts);
	gst_ccnxsink_send( me, data, size, ts );
	gst_buffer_unref( buffer );
  }
  
}

static GstTask	*eventTask;
static GMutex	*eventLock;
static GCond	*eventCond;
static GStaticRecMutex	task_mutex = G_STATIC_REC_MUTEX_INIT;

static void
ccn_event_thread(void *data) {
  Gstccnxsink* me = (Gstccnxsink*) data;
  struct ccn_charbuf *temp;
  int res = 0;

  GST_DEBUG ("CCNxSink event: *** event thread starting");
  
    temp = ccn_charbuf_create();

  if( (me->ccn_closure = calloc(1, sizeof(struct ccn_closure))) == NULL ) {
    GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("closure alloc failed"));
    return;
  }

  me->ccn_closure->data = me;
  me->ccn_closure->p = new_interests;
  me->i_size = LONG_MAX;
  me->timeouts = 0;
  
  /* Set up a handler for interests */
  ccn_set_interest_filter(me->ccn, me->name, me->ccn_closure);
  GST_DEBUG ("CCNxSink event: interest filter registered\n");

  temp->length = 0;
  ccn_uri_append(temp, me->name->buf, me->name->length, TRUE);
  GST_DEBUG ("CCNxSink event: using uri: %s\n", ccn_charbuf_as_string(temp));
  
  
  while (res >= 0) {
      GST_DEBUG ("CCNxSink event: *** looping");
      res = ccn_run(me->ccn, 50);
	  check_fifo(me);
      if (res < 0 && ccn_get_connection_fd(me->ccn) == -1) {
          GST_DEBUG ("CCNxSink event: need to reconnect...");
          /* Try reconnecting, after a bit of delay */
          msleep((30 + (pid() % 30)) * 1000);
          res = ccn_connect(me->ccn, ccndHost() );
      }
  }
  GST_DEBUG ("CCNxSink event: *** event thread ending");
}


/* create a connection for sending data */
static gboolean
gst_ccnxsink_start (GstBaseSink * bsink)
{
  Gstccnxsink *me;

  gboolean b_ret = FALSE;

  me = GST_CCNXSINK (bsink);
  me->temp = ccn_charbuf_create();
  me->lastPublish = ccn_charbuf_create();
  
  GST_DEBUG ("CCNxSink: starting, getting connections");

  GST_DEBUG ("CCNxSink: step 1, %s", me->uri);

  setup_ccn( me );
  GST_DEBUG ("CCNxSink: ccn is setup");

  eventTask = gst_task_create( ccn_event_thread, me );
  if( NULL == eventTask ) {
      GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("creating event thread failed"));
      return FALSE;
  }
  me->fifo_cond = g_cond_new();
  me->fifo_lock = g_mutex_new();
  gst_task_set_lock( eventTask, &task_mutex );
  eventCond = g_cond_new();
  eventLock = g_mutex_new();
  b_ret = gst_task_start(eventTask);
  if( FALSE == b_ret ) {
      GST_ELEMENT_ERROR( me, RESOURCE, READ, (NULL), ("starting event thread failed"));
      return FALSE;
  }
  GST_DEBUG ("CCNxSink: event thread started");

  return TRUE;
}


static gboolean
gst_ccnxsink_stop (GstBaseSink * bsink)
{
  Gstccnxsink *me = GST_CCNXSINK (bsink);
  
  if( me->i_bufoffset ) fifo_put( me, me->buf, TRUE );

  GST_DEBUG ("stopping, closing connections");

  return TRUE;
}

static gboolean
gst_ccnxsink_set_uri (Gstccnxsink * me, const GValue * gv)
{
  gchar *protocol;
  const gchar *uri;
  
  uri = g_value_get_string( gv );
	GST_DEBUG("CCNxSink: setting uri: %s\n", uri);
  
  if( NULL == uri ) {
    g_free (me->uri);
    me->uri = g_strdup(CCNX_DEFAULT_URI);
    return TRUE;
  }

  protocol = gst_uri_get_protocol (uri);
  if( ! protocol ) goto wrong_protocol;
  if (strcmp (protocol, "ccnx") != 0)
    goto wrong_protocol;
  g_free (protocol);

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

static void
gst_ccnxsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstccnxsink *me = GST_CCNXSINK (object);

	GST_DEBUG("CCNxSink: set property: %d\n", prop_id);
  switch (prop_id) {
    case PROP_URI:
      gst_ccnxsink_set_uri( me, value );
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
gst_ccnxsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstccnxsink *me = GST_CCNXSINK (object);

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

static void
gst_ccnxsink_finalize(GObject *obj) {

  Gstccnxsink *me;

  me = GST_CCNXSINK (obj);

  if (me->caps)
    gst_caps_unref (me->caps);
  g_free (me->uri);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}
