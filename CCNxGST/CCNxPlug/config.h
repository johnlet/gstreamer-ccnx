/*
 * GStreamer, CCNx Plug-in
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

#ifndef CONFIG_H
#define CONFIG_H


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


#endif /* CONFIG_h */