/** \file CCNxPlugin.h
 * \brief Simple header used in the ccnx plug-in definition
 */
/*
 * GStreamer-CCNx, interface GStreamer media flow with a CCNx network
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
