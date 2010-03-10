/** \file CCNxPlugin.h
 * \brief Simple header used in the ccnx plug-in definition
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

#ifndef CCNXPLUGIN_H
#define CCNXPLUGIN_H

#ifdef WIN32
/*
 * The following ifdef block is the standard way of creating macros which make exporting 
 * from a DLL simpler. All files within this DLL are compiled with the TESTPLUG_EXPORTS
 * symbol defined on the command line. this symbol should not be defined on any project
 * that uses this DLL. This way any other project whose source files include this file see 
 * TESTPLUG_API functions as being imported from a DLL, whereas this DLL sees symbols
 * defined with this macro as being exported.
 */
#ifdef TESTPLUG_EXPORTS
#define TESTPLUG_API __declspec(dllexport)
#else
#define TESTPLUG_API __declspec(dllimport)
#endif

#endif // WIN32

#endif // CCNXPLUGIN_H