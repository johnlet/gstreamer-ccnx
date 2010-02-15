// TestPlug.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "CCNxPlugin.h"


/*
 * CCNx Plug-in
 * Copyright (C) 2010 John Letourneau <topgun@bell-labs.com>
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

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


/** TODO: These headers should match those for each of the elements */
#include "ccnxsrc.h"
#include "ccnxsink.h"

unsigned int  _get_output_format(void) { return 0; }
/*
TESTPLUG_API
int _getaddrinfo(const char* host, const char* port, const struct addrinfo *hints, struct addrinfo **res) {
      /* return getaddrinfo( host, port, hints, res );
	return 0;
}
*/
/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
plug_init (GstPlugin * ccnx)
{
	fprintf( stderr, "I am in plug-init\n" );
  if( ! gst_element_register (ccnx, "ccnxsrc", GST_RANK_NONE,
      GST_TYPE_CCNXSRC) ) return FALSE;
  if( ! gst_element_register (ccnx, "ccnxsink", GST_RANK_NONE,
      GST_TYPE_CCNXSINK) ) return FALSE;
      
  return TRUE;
}


/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "testplugin"
#define VERSION "1.0"
#endif

/* gstreamer looks for this structure to register plug-in elements
 *
 */

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "testplug",
    "Interface data content to-from a TEST CCNx network",
    plug_init,
    VERSION,
    "LGPL",
    "I@D",
    "http://bell-labs.com/"
)
