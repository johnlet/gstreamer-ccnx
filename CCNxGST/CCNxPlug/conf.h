/** \file conf.h
 * \brief Collection of common configuration parameters and environment peculiars
 *
 * \date Created Jan, 2010
 * \author John Letourneau <topgun@bell-labs.com>
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

#ifndef CONF_H
#define CONF_H

/**
 * Size of the FIFO queue used between the pipeline and the network
 */
#define CCNX_SINK_FIFO_MAX	20

/**
 * Size of the FIFO queue used between the network and the pipeline
 */
#define CCNX_SRC_FIFO_MAX	5

#ifdef WIN32
#  include <windows.h>
   typedef long ssize_t;

   struct timespec {
	   long			tv_sec;
	   long			tv_nsec;
   };
   static int nanosleep( const struct timespec *tv, struct timespec *rm ) {
	   DWORD tm = tv->tv_sec * 1000;
	   tm += tv->tv_nsec / 1000000;
	   Sleep( tm );
	   return 0;
   }

#  include <process.h>
   /** TODO fix this */
/* #  define getpid()	_getpid() */
   static int pid() { return 1000; }

#  define CLOCK_REALTIME	0
   static int clock_gettime( int clk, struct timespec *tm ) {
	   LARGE_INTEGER freq;
	   LARGE_INTEGER val;
	   if( QueryPerformanceCounter( &val )
		   && QueryPerformanceFrequency( &freq ) ) {
			   tm->tv_sec = val.QuadPart / freq.QuadPart / 1000000000;
			   tm->tv_nsec = (val.QuadPart / freq.QuadPart) % 1000000000;
			   return 0;
	   } else {
		   tm->tv_sec = 0;
		   tm->tv_nsec = 0;
		   return 0;
	   }
   }

#endif


#endif /* CONF_H */
