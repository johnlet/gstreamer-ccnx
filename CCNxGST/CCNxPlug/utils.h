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

#ifndef UTILS_H
#define UTILS_H

#include <ccn/ccn.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <stdlib.h>

/*
 * Gets the host where we can find the ccnd router process.
 * If none has been configured, a null is returned.
 *
 * We define an environment variable for the host name. If you want the port, CCN already
 * has a #define CCN_LOCAL_PORT_ENVNAME "CCN_LOCAL_PORT". The default if not set is
 * #define CCN_DEFAULT_UNICAST_PORT "9695" as of this writing. See ccn/ccnd.h.
 */
#define CCND_HOST_ENV_VAR		"CCND_HOST"
#define CCN_KEYSTORE_ENV_VAR	"CCN_KEYSTORE"
#define CCN_PASSPHRASE_ENV_VAR	"CCN_PASSPHRASE"

char* ccndHost();

int loadKey( struct ccn *ccn, struct ccn_signing_params *sp );

struct ccn_keystore* fetchStore();

struct ccn_charbuf* makeLocator( const struct ccn_pkey* key );

/*
 * Create a charbuf containing the interest, making it look like a URI:
 * /test/part2/details/person.
 * We make no claim as to the validity of this format.
 */
struct ccn_charbuf* interestAsUri( const struct ccn_upcall_info * info );


/*
 * Dump routines.
 * The output is of general form:
 *
 * Address   word  word  word  word  <ASCII representation>
 *
 * Address octal or hex format of the addresses being dumped.
 * The values are rounded down so the addresses look nicer.
 * This means that the first line may skip some of the bytes to dump because
 * they are before the requested address.
 *
 * Each 'word' contains 4 bytes [ok, I'm on a 32 bit machine].
 *
 * If no printable ASCII exists for a byte, a '.' appears in that spot.
 */
 
typedef unsigned char* DumpAddr_t;
typedef int DumpSize_t;

#define DUMP_ADDR(addr) (const DumpAddr_t)(addr)
#define DUMP_SIZE(sz)   (const DumpSize_t)(sz)

/*
 * Dump memory in octal format.
 */
void oDump( const DumpAddr_t ptr, const DumpSize_t size );

/*
 * Dump memory in hex format.
 */
void hDump( const DumpAddr_t ptr, const DumpSize_t size );

/*
 * Dump out components of a name.
 * Given the buffer holding the name, and the number of components to dump out,
 * we parse the name and send dumped output to stderr.
 */
void
compDump(struct ccn_charbuf *cbuf, int todo);

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
          );

#endif


