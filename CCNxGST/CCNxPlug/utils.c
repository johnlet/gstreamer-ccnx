#include "StdAfx.h"

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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

char*
ccndHost() {
  return getenv( CCND_HOST_ENV_VAR );
}

int
loadKey( struct ccn *ccn, struct ccn_signing_params *sp ) {
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
		memcpy( sp->pubid, pubid->buf, sizeof(sp->pubid) );
		ccn_default_pubid(ccn, &sp);
	}
	ccn_charbuf_destroy(&pubid);
	return rc;
}

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

/*
 * Look at an upcall_info and reassemble an interest into a URI looking thing.
 * We put everything into a charbuf, letting the caller release the buffer when done.
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


/*
 * Dump routines. We can dump out a piece of memory in either hex or octal format.
 */
 
void
commonDump( const DumpAddr_t ptr, const DumpSize_t size, char* addrFmt, char* byteFmt, char* pad ) {
  DumpAddr_t cp = ptr;
  int sz = 0;
  int wd = 0;
  int byt = 0;
  int idx = 0;
  char chrs[24];
  
  /* Round down the address to be printed */
  cp = (DumpAddr_t)((int)cp & 0xFFFFFFF8);
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

void
oDump( const DumpAddr_t ptr, const DumpSize_t size ) {
  commonDump( ptr, size, "%011o  ", "%03o", "%3s" );
}

void
hDump( const DumpAddr_t ptr, const DumpSize_t size ) {
  commonDump( ptr, size, "%08X  ", "%02X", "%2s" );
}

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


/*
 * Compare stuff.
 * If i is >0, only i positions are checked.
 * Returns:
 * -1    :: the two are equivalent
 * n    :: they differ at position n, starting position is 0
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
