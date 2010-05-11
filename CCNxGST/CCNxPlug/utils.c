/** \file utils.c
 * \brief Utility functions for the GStreamer/CCNx plug-in code
 *
 * \author John Letourneau <topgun@bell-labs.com>
 *
 * \date Created Nov, 2009
 */
/*
 * GStreamer-CCNx, utility functions
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


const char* COPYRIGHT = " \
 * \
 * GStreamer-CCNx, interface GStreamer media flow with a CCNx network \
 * Copyright (C) 2009 Alcatel-Lucent Inc, and John Letourneau <topgun@bell-labs.com> \
 * \
 * This library is free software; you can redistribute it and/or \
 * modify it under the terms of the GNU Library General Public \
 * License as published by the Free Software Foundation; \
 * version 2 of the License. \
 * \
 * Permission is hereby granted, free of charge, to any person obtaining a \
 * copy of this software and associated documentation files (the 'Software'), \
 * to deal in the Software without restriction, including without limitation \
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, \
 * and/or sell copies of the Software, and to permit persons to whom the \
 * Software is furnished to do so, subject to the following conditions: \
 * \
 * The above copyright notice and this permission notice shall be included in \
 * all copies or substantial portions of the Software. \
 * \
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR \
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, \
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE \
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER \
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING \
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER \
 * DEALINGS IN THE SOFTWARE. \
 * \
 * You should have received a copy of the GNU Library General Public \
 * License, License.txt, along with this library; if not, write to the Free \
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA \
 * \
 * \
 ";

#ifdef WIN32
#include "StdAfx.h"
#endif

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "conf.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include <ccn/ccn.h>

/**
 * Retrieve the host name where the ccnd router is located
 *
 * Each client must contact a router in order to have their request serviced.
 * We define an environment variable for the host name. If you want the port, CCN already
 * has a \#define CCN_LOCAL_PORT_ENVNAME "CCN_LOCAL_PORT". The default if not set is
 * \#define CCN_DEFAULT_UNICAST_PORT "9695" as of this writing. See ccn/ccnd.h.
 *
 *
 * \return pointer to the name of the host, NULL if not set
 */
char*
ccndHost() {
  return getenv( CCND_HOST_ENV_VAR );
}

/**
 * Locate and load a client's security keys
 *
 * A check is made to see if the user has set their environment variable indicating
 * the keystore file that should be used for this process. The passphrase is also
 * an environment variable.
 *
 * If the environment variable is not set, a default location is attempted:
 * $HOME/.ccnx/.ccnx_keystore with a passphrase of passw0rd [that is a zero, not an o]
 *
 * \return 0 on success, -1 otherwise
 */

int
loadKey( struct ccn *ccn, const struct ccn_signing_params *sp ) {
	int rc;
	char* str;
	struct ccn_charbuf* pubid;

	pubid = ccn_charbuf_create();
	str = getenv( CCN_KEYSTORE_ENV_VAR );
	if( str ) {
		char* pwd = getenv( CCN_PASSPHRASE_ENV_VAR );
		rc = ccn_load_private_key( ccn, str, pwd, pubid );
		if (rc != 0) {
			fprintf(stderr, "Failed to load keystore: %s\n", str);
			ccn_charbuf_destroy(&pubid);
			return rc;
		}
	} else {
		struct ccn_charbuf* temp = ccn_charbuf_create();
		temp->length = 0;
		ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
		rc = ccn_load_private_key( ccn, ccn_charbuf_as_string(temp), "passw0rd", pubid );
		if (rc != 0) {
			fprintf(stderr, "Failed to load default keystore: %s\n", ccn_charbuf_as_string(temp));
			ccn_charbuf_destroy( &temp );
			ccn_charbuf_destroy(&pubid);
			return rc;
		}
		ccn_charbuf_destroy( &temp );
	}
	if( rc == 0 && pubid->length == sizeof(sp->pubid) ) {
		memcpy( (char*)sp->pubid, pubid->buf, sizeof(sp->pubid) );
		ccn_default_pubid(ccn, sp);
	}
	ccn_charbuf_destroy(&pubid);
	return rc;
}

/**
 * Locate and load a client's security keys; this may be deprecated
 *
 * A check is made to see if the user has set their environment variable indicating
 * the keystore file that should be used for this process. The passphrase is also
 * an environment variable.
 *
 * If the environment variable is not set, a default location is attempted:
 * $HOME/.ccnx/.ccnx_keystore with a passphrase of passw0rd [that is a zero, not an o]
 *
 * \return a populated structure with the keys loaded into it
 */
struct ccn_keystore*
fetchStore() {
	char* str;
	int rc;
	struct ccn_keystore* ans = NULL;

	ans = ccn_keystore_create();
	str = getenv( CCN_KEYSTORE_ENV_VAR );
	if( str ) {
		char* pwd = getenv( CCN_PASSPHRASE_ENV_VAR );
		rc = ccn_keystore_init( ans, str, pwd );
		if (rc != 0) {
			fprintf(stderr, "Failed to initialize keystore: %s\n", str);
			ccn_keystore_destroy(&ans);
			return NULL;
		}
	} else {
		struct ccn_charbuf* temp = ccn_charbuf_create();
		temp->length = 0;
		ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
		rc = ccn_keystore_init(ans,
							ccn_charbuf_as_string(temp),
							"passw0rd");
		if (rc != 0) {
			fprintf(stderr, "Failed to initialize keystore: %s\n", ccn_charbuf_as_string(temp));
			ccn_charbuf_destroy( &temp );
			ccn_keystore_destroy(&ans);
			return NULL;
		}
		ccn_charbuf_destroy( &temp );
	}
	return ans;
}

/**
 * Creates a key locator portion of an interest
 * [ \todo or is that for published content?]
 *
 * \param key		pointer to the key structure
 * \return character buffer encoded with the \<KEYLOCATOR\> \<KEY>pkey</KEY\> \</KEYLOCATOR\>
 */
struct ccn_charbuf*
makeLocator( const struct ccn_pkey* key ) {
	int rc;
	struct ccn_charbuf* ans = ccn_charbuf_create();
	ans->length = 0;
	
    ccn_charbuf_append_tt(ans, CCN_DTAG_KeyLocator, CCN_DTAG);
    ccn_charbuf_append_tt(ans, CCN_DTAG_Key, CCN_DTAG);
    rc = ccn_append_pubkey_blob(ans, key);
    if (rc < 0)
        ccn_charbuf_destroy(&ans);
    else {
        ccn_charbuf_append_closer(ans); /* </Key> */
        ccn_charbuf_append_closer(ans); /* </KeyLocator> */
    }
	return ans;
}

 
/**
 * Create a charbuf containing the interest, making it look like a URI
 *
 * An example of the kind of URI we expect is: /test/part2/details/person.
 * We make no claim as to the validity of this format.
 *
 * \param info		Structure holding the components to be assembled
 * \return character buffer with the URI string in it; the caller is responsible for destroying it when done
 */
struct ccn_charbuf*
interestAsUri( const struct ccn_upcall_info * info ) {
  struct ccn_charbuf* cb;
  struct ccn_indexbuf *comps;
  int last;
  int i;
  
  cb = ccn_charbuf_create();
  comps = info->interest_comps;
  last = comps->n;
  last--;
  ccn_charbuf_reserve( cb, comps->buf[last] );
  
  for( i=0; i<last; ++i ) {
    size_t start = 2 + comps->buf[i];
    const unsigned char* cp = info->interest_ccnb + start;
    ccn_charbuf_append_string( cb, "/" );
    ccn_charbuf_append_string( cb, (const char*)cp );
  }
  
  return cb;
}

/**
 * Function to sleep for a specified number of milli-seconds
 *
 * \param msecs		number of msecs to sleep
 */
void
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

/**
 * Common routine for dumping out octal or hex
 *
 * Very little is different about dumping out memory with different number bases. Those
 * few things that are different can be sent in as paramaters as they are here.
 * 
 * The address we print is actually rounded down so that it ends in a nice value, like '0'.
 * This actually makes looking for data easier since the math is easier when you have a nice
 * value such as this.
 *
 * \param ptr		points to the memory to be dumped
 * \param size		number of bytes to dump
 * \param addrFmt	the printf format string to use in creating the address output
 * \param byteFmt	the printf format string used in creating the data for output
 * \param pad		the printf format string used in padding output for non-displayed memory
 */
static void
commonDump( const DumpAddr_t ptr, const DumpSize_t size, char* addrFmt, char* byteFmt, char* pad ) {
  DumpAddr_t cp = ptr;
  int sz = 0;
  int wd = 0;
  int byt = 0;
  int idx = 0;
  char chrs[24];

#if __WORDSIZE == 64
	long mask = 0xFFFFFFFFFFFFFFF8;
#else
	long mask = 0xFFFFFFF8;
#endif  
  /* Round down the address to be printed */
  cp = (DumpAddr_t)((long)cp & mask);
  fprintf(stderr, addrFmt, cp);
  
  /* Now add padding for those bytes not asked for */
  while( cp < ptr ) {
    cp++;
    fprintf(stderr, pad, " ");
    chrs[idx++] = '.';
    if( ++byt < 4 ) continue;
    byt = 0;
    wd++;
    fprintf(stderr, "  ");
  }
  
  /* Start printing the bytes asked for, adding line breaks as needed */
  /* With each line break, we output the ASCII to end a line, and start */
  /* the next line with the address of that memory. */
  while( sz++ < size ) {
    fprintf(stderr, byteFmt, *cp);
    if( *cp >= ' ' && *cp <= '~' ) chrs[idx++] = *cp;
    else chrs[idx++] = '.';
    cp++;
    if( ++byt < 4 ) continue;
    byt = 0;
    fprintf(stderr, "  ");
    if( ++wd < 4 ) continue;
    wd = 0;
    chrs[idx] = '\0';
    fprintf(stderr, "<%s>\n", chrs);
    idx = 0;
    if( sz < size ) fprintf(stderr, addrFmt, cp);
  }
  
  /* If we have a partial line, pad the rest and print the ASCII */
  if(idx > 0) {
    while( wd < 4 ) {
      fprintf(stderr, pad, " ");
      chrs[idx++] = '.';
      if( ++byt < 4 ) continue;
      byt = 0;
      wd++;
      fprintf(stderr, "  ");
    }
    chrs[idx] = '\0';
    fprintf(stderr, "<%s>\n", chrs);
  }
}

/**
 * Octal dump routine.
 *
 * The output is of general form:
 * \code
 * Address   word  word  word  word  <ASCII representation>
 * \endcode
 * <table>
 * <tr>
 *   <td>Address</td>
 *	 <td>
 *	 octal format of the addresses being dumped.
 *   The values are rounded down so the addresses look nicer.
 *   This means that the first line may skip some of the bytes to dump because
 *   they are before the requested address.
 *	 </td>
 * </tr>
 * <tr>
 *	 <td>word</td>
 *	 <td>
 *	 contains 4 bytes [ok, I'm on a 32 bit machine].
 *	 This too is in octal.
 *	 </td>
 * </tr>
 * <tr>
 *	 <td>ASCII</td>
 *	 <td>
 *	 Prints the character if it makes sense to.
 *	 If no printable ASCII exists for a byte, a '.' appears in that spot.
 *	 Don't be fooled by printing a period vs. an unprintable character.
 *	 </td>
 * </tr>
 * </table>
 *
 * \param ptr		points to the memory to be dumped
 * \param size		the number of bytes to dump
 */

void
oDump( const DumpAddr_t ptr, const DumpSize_t size ) {
  commonDump( ptr, size, "%011o  ", "%03o", "%3s" );
}
/**
 * Hex dump routine.
 *
 * The output is of general form:
 * \code
 * Address   word  word  word  word  <ASCII representation>
 * \endcode
 * <table>
 * <tr>
 *   <td>Address</td>
 *	 <td>
 *	 hex format of the addresses being dumped.
 *   The values are rounded down so the addresses look nicer.
 *   This means that the first line may skip some of the bytes to dump because
 *   they are before the requested address.
 *	 </td>
 * </tr>
 * <tr>
 *	 <td>word</td>
 *	 <td>
 *	 contains 4 bytes [ok, I'm on a 32 bit machine].
 *	 This too is in hex.
 *	 </td>
 * </tr>
 * <tr>
 *	 <td>ASCII</td>
 *	 <td>
 *	 Prints the character if it makes sense to.
 *	 If no printable ASCII exists for a byte, a '.' appears in that spot.
 *	 Don't be fooled by printing a period vs. an unprintable character.
 *	 </td>
 * </tr>
 * </table>
 *
 * \param ptr		points to the memory to be dumped
 * \param size		the number of bytes to dump
 */
void
hDump( const DumpAddr_t ptr, const DumpSize_t size ) {
  commonDump( ptr, size, "%08X  ", "%02X", "%2s" );
}

/**
 * Dump out components
 *
 * Dump out components of a name.
 * Given the buffer holding the name, and the number of components to dump out,
 * we parse the name and send dumped output to stderr.
 *
 * \param cbuf		character buffer holding the name
 * \param todo		number of components to dump out
 */
void
compDump(struct ccn_charbuf *cbuf, int todo) {
  int rc;
  int i;
  struct ccn_indexbuf* comps;
  struct ccn_parsed_ContentObject obj = {0};
  comps = ccn_indexbuf_create();
  rc = ccn_parse_ContentObject(cbuf->buf, cbuf->length, &obj, comps);
  if (rc >= 0) {
    for( i=0; i<todo; ++i ) {
      const unsigned char *cp;
      size_t sz;
      fprintf(stderr, "%3d: ", i);
      if( 0 > ccn_name_comp_get( cbuf->buf, comps, i, &cp, &sz ) )
        fprintf(stderr, "could not get comp\n");
      else
        hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }
  }
  ccn_indexbuf_destroy(&comps);
}

uintmax_t ccn_ccnb_fetch_segment( const unsigned char* buf, const struct ccn_indexbuf* idx) {
  const unsigned char *cp;
  size_t sz;
  uintmax_t ans = 0;
  int i;

  if( NULL == buf || NULL == idx ) return -1;

  if( 0 > ccn_name_comp_get( buf, idx, idx->n-2, &cp, &sz ) )
    return -1;
  for( i=1; i<sz; ++i ) ans = (ans<<8) + cp[i]; // skip first byte; marker
  return ans;
}

uintmax_t
ccn_charbuf_fetch_segment(const struct ccn_charbuf *name) {
    struct ccn_indexbuf *nix = ccn_indexbuf_create();
    int n = ccn_name_split(name, nix);
    struct ccn_buf_decoder decoder;
    struct ccn_buf_decoder *d;
    size_t lc;
    size_t oc;
    size_t size;
    const unsigned char *comp;

    if(n >= 1) {
      oc = nix->buf[n-1];
      lc = nix->buf[n] - oc;
      d = ccn_buf_decoder_start(&decoder, name->buf + oc, lc);
      if (ccn_buf_match_dtag(d, CCN_DTAG_Component)) {
	      ccn_buf_advance(d);
	      if (ccn_buf_match_blob(d, &comp, &size)) {
	        uintmax_t ans = 0;
	        int i;
	        for( i=1; i<size; ++i ) ans = (ans<<8) + comp[i]; // skip first byte; marker
	        return ans;
	      }
      }
    }
    return -1;
}

void
show_comps( const unsigned char* buf, const struct ccn_indexbuf* idx ) {
    int i;
    int end;
    
    if( ! idx ) return;
    if( ! buf ) return;
    
    end = idx->n - 1; /* always skip the trailing %00 tag */
    
    for( i=0; i<end; ++i ) {
      const unsigned char *cp;
      size_t sz;
      fprintf(stderr, "%3d: ", i);
      if( 0 > ccn_name_comp_get( buf, idx, i, &cp, &sz ) )
        fprintf(stderr, "could not get comp\n");
      else
        hDump( DUMP_ADDR( cp ), DUMP_SIZE( sz ) );
    }

}


/**
 * Compare stuff being held in two character arrays and index buffers
 *
 * The character arrays are parts of character buffers. They do not represent
 * strings in the strict sense; they may contain binary data and include
 * null characters. The index information stipulates where the data really is
 * and how large it is.
 *
 * \param data1		pointer to the first character array
 * \param indexbuf1	index buffer into the first characgter array
 * \param data2		pointer to the second character array
 * \param indexbuf2	index buffer into the second character array
 * \param i			if >0, only i positions are checked
 * \return at which component they differ
 * \retval -1	the two are equivalent
 * \retval n	the two differ at position n, starting position is 0
 */
int
name_compare(const unsigned char *data1,
                  const struct ccn_indexbuf *indexbuf1,
                  const unsigned char *data2,
                  const struct ccn_indexbuf *indexbuf2,
                  unsigned int i
          ) {
  unsigned int checked;
  int loop;
  int rc1;
  int rc2;
  const unsigned char *cp1, *cp2;
  size_t sz1, sz2;
  
  for( checked=0, loop=1; loop; ) {
    rc1 = ccn_name_comp_get( data1, indexbuf1, checked, &cp1, &sz1 );
    rc2 = ccn_name_comp_get( data2, indexbuf2, checked, &cp2, &sz2 );
    if( rc1 == -1 && rc2 == -1 ) {
      checked = -1; /* They are equal */
      break;
    }
    if( rc1 != rc2 ) {
      break; /* They differ */
    }
    if( sz1 != sz2 ) {
      break; /* They differ */
    }
    if( memcmp( cp1, cp2, sz1 ) ) {
      break; /* They differ */
    }
    ++checked;
    if( i > 0 ) loop = checked < i;
  }
  return checked;
}
