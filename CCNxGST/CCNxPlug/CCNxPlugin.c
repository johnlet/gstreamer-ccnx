/** \file CCNxPlugin.c
 * \brief Implements the GStreamer entry point for the shared library plug-in
 *
 * \date Created Nov, 2009
 * \author John Letourneau <topgun@bell-labs.com>
 */

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

#include "ccnxsrc.h"
#include "ccnxsink.h"

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * Helps with Windows compatibility
 */
unsigned int  _get_output_format(void) { return 0; }

#endif


/**
 * Entry point to initialize the plug-in
 *
 * Initialize the plug-in itself,
 * and register the element factories.
 */
gboolean
plug_init (GstPlugin * ccnx)
{
  if( ! gst_element_register (ccnx, "ccnxsrc", GST_RANK_NONE,
      GST_TYPE_CCNXSRC) ) return FALSE;
  if( ! gst_element_register (ccnx, "ccnxsink", GST_RANK_NONE,
      GST_TYPE_CCNXSINK) ) return FALSE;
      
  return TRUE;
}


#ifndef PACKAGE

/**
 * Identifies the name of the package
 *
 * This is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#define PACKAGE "CCNxPlugin"

/**
 * Identifies the version of the package
 *
 * This is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#define VERSION "0.1"
#endif

/**
 * Structure used by the  GStreamer framework to register plug-in elements
 *
 */

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    PACKAGE,
    "Interface data content to-from a CCNx network",
    plug_init,
    VERSION,
    "LGPL",
    "I@D",
    "http://bell-labs.com/"
)


/**
 * \date Created: Nov, 2009
 * \author John Letourneau <topgun@bell-labs.com>
 *
 * \mainpage Content Centric Network GStreamer Plug-in
 *
 * This plug-in's entry point is implemented in CCNxPlugin.c
 *
 * The
 * <a href="http://www.ccnx.org">Content Centric Network Project</a> <b>[CCN or CCNx]</b>
 * is an effort by a group at Xerox PARC to redefine
 * how the internet is used; to a level of changing the content and how it is requested.
 *
 * <a href="http://www.gstreamer.net">GStreamer</a> <b>[GST]</b>
 * is a framwork that allows for a simplified means of defining media processing pipelines.
 *
 * Our effort here is to marry these two together into a GST plug-in that allows for a media pipeline
 * to read its source from, or provide its content to a CCNx style network.
 * There are two elements that have been implemented: ccnxsrc and ccnxsink.
 *
 * The ccnxsink GStreamer element is responsible for consuming packets from
 * a GST pipeline and delivering them onto a CCNx network
 * for consumption by other clients wanting to read such published data;
 * via the partner element, ccnxsrc.
 * Both of these elements take an attribute, \em uri, which is used to name the
 * content; one while being published, the other to signify what content the
 * client is interested in receiving.
 * The protocol specified for the uri is specific to this kind of network:
 * [more information on naming can be found at \ref SINKCCNNAMING ]
 *
 * \code
 *
 * ccnx://a/path/naming/the/content/from/global/to/more/specific
 * \endcode
 * These elemnts operate in a very simple manner:
 * read data, packatize as appropriate, send data.
 * The packet or message size for the network is different than that of the pipeline;
 * althought each can vary according to the application, in our case we have
 * configured each as being different from the other.
 *
 * This is the product of a research project.
 * As such there are potentially faults and pitfalls within the code,
 * in particular areas dealing with error processing.
 * It is often the case that client process failure is seen as an acceptable
 * means of reacting to an error condition,
 * thus it may not be acceptable to use this code in a production environment.
 *
 * More details on the lower level designs can be found at:
 * \li \subpage CCNSRCDESIGN
 * \li \subpage CCNSINKDESIGN
 *
 * \section CCNXPLUGINEXAMPLE Example Usage of the CCNx Elements
 *
 * Here is a script showing the environment setup and invocation of a GST
 * pipeline. The source of this pipeline is the camera on the laptop;
 * the last element in the pipeline is the ccnxsink element:
 * \code
#!/bin/bash
#
# Send out camera input through a CCNx network
# Used on a Windows XP system within an MSYS shell environment

CCND_HOST=gryffindor.research.bell-labs.com

CCN_KEYSTORE=/home/immersion/.ccnx/.ccnx_keystore
CCN_PASSPHRASE=passw0rd

export CCND_HOST CCN_KEYSTORE CCN_PASSPHRASE

# MSYS shell seems to handle some of the inputs in an odd way.
# Note the format of the uri. If we try ccnx:/windows/1, which works on
# Linux, then it fails on windows because the shell we are using wants
# to substitute the leading '/' with C:\path\to\msys\home. What a pain!
# Luckily the following works and ccn does not have a problem with it.

gst-launch dshowvideosrc ! \
        video/x-raw-yuv, width=320, height=240, framerate=20/1 ! \
        ffmpegcolorspace ! \
        theoraenc ! \
        oggmux ! \
        ccnxsink uri=ccnx:///com/btl/gc/test/windows/1

 * \endcode
 * The first several elements of this pipeline take the video and effectively package
 * it up into an ogg container. Some compression is taking place; raw video takes
 * many bytes to transmit.
 *
 * The ccnxsink element will take its attribute, uri, and use that when publishing
 * its content onto the network.
 *
 * A similar example can be drawn up for the ccnxsrc element, one where
 * the source element on the pipeline reads the stream created from the
 * above pipeline, and renders it onto the user's display:
 * \code
#!/bin/bash

CCND_HOST=pollux.research.bell-labs.com

CCN_KEYSTORE=/home/immersion/.ccnx/.ccnx_keystore
CCN_PASSPHRASE=passw0rd

export CCND_HOST CCN_KEYSTORE CCN_PASSPHRASE


gst-launch ccnxsrc uri=ccnx:///windows/1 ! \
        decodebin ! \
        ffmpegcolorspace ! dshowvideosink

immersion@IMMERSION4 /c/projects/gstreamer-ccnx/CCNxGST/Release
$
 * \endcode
 */