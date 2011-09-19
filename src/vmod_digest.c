#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <mhash.h>
#undef assert

#include "vrt.h"
#include "bin/varnishd/cache.h"
#include "vsha256.h"
#include "vcc_if.h"

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}



/* C89 compliant way to cast 'char' to 'unsigned char'. */
static inline unsigned char
to_uchar (char ch)
{
  return ch;
}

/*
 * Base64 encode IN array of size INLEN into OUT array of size OUTLEN.
 * If OUTLEN is less than BASE64_LENGTH(INLEN), write as many bytes as
 * possible.  If OUTLEN is larger than BASE64_LENGTH(INLEN), also zero
 * terminate the output buffer.
 *
 * Borrowed from, err, Simon Josefsson and the FSF
 * http://cvs.savannah.gnu.org/viewvc/gnulib/gnulib/lib/base64.c?view=markup&content-type=text%2Fvnd.viewcvs-markup&revision=HEAD
 * FIXME: FIX...
 */
static size_t
base64_encode (const char *restrict in, size_t inlen,
		char *restrict out, size_t outlen)
{
	static const char b64str[64] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t outlenorig = outlen;

	while (inlen && outlen)
	{
		*out++ = b64str[(to_uchar (in[0]) >> 2) & 0x3f];
		if (!--outlen)
			break;
		*out++ = b64str[((to_uchar (in[0]) << 4)
				+ (--inlen ? to_uchar (in[1]) >> 4 : 0))
			& 0x3f];
		if (!--outlen)
			break;
		*out++ =
			(inlen
			 ? b64str[((to_uchar (in[1]) << 2)
				 + (--inlen ? to_uchar (in[2]) >> 6 : 0))
			 & 0x3f]
			 : '=');
		if (!--outlen)
			break;
		*out++ = inlen ? b64str[to_uchar (in[2]) & 0x3f] : '=';
		if (!--outlen)
			break;
		if (inlen)
			inlen--;
		if (inlen)
			in += 3;
	}

	if (!outlen)
		return -1;
	outlen--;
	*out = '\0';
	return outlenorig-outlen;
}

static const char *
vmod_hmac_generic(struct sess *sp, hashid hash, const char *key, const char *msg)
{
	size_t maclen = mhash_get_hash_pblock(hash);
	size_t blocksize = mhash_get_block_size(hash);
	unsigned char mac[blocksize];
	unsigned char hexenc[2*blocksize+3]; // 0x, '\0' + 2 per input
	unsigned char *hexptr, *p;
	int j, u;
	MHASH td;

	/*
	 * XXX: From mhash(3):
	 * size_t mhash_get_hash_pblock(hashid type);
	 *     It returns the block size that the algorithm operates. This
	 *     is used in mhash_hmac_init. If the return value is 0 you
	 *     shouldn't use that algorithm in  HMAC.
	 */
	assert(mhash_get_hash_pblock(hash) > 0);

	td = mhash_hmac_init(hash, (void *) key, strlen(key),
		mhash_get_hash_pblock(hash));
	mhash(td, msg, strlen(msg));
	mhash_hmac_deinit(td,mac);
	
	/*
	 * HEX-encode
	 */
	hexptr = hexenc;
	sprintf(hexptr,"0x");
	hexptr+=2;
	for (j = 0; j < blocksize; j++) {
		sprintf(hexptr,"%.2x", mac[j]);
		hexptr+=2;
		assert((hexptr-hexenc)<sizeof(hexenc));
	}
	*hexptr = '\0';

	/*
	 * Base64 encode on the workspace (since it's returned).
	 */
	u = WS_Reserve(sp->ws,0);
	p = sp->ws->f;
	u = base64_encode(hexenc,strlen(hexenc),p,u);
	/*
	 * Not enough space.
	 */
	if (u < 0) {
		WS_Release(sp->ws,0);
		return NULL;
	}
	WS_Release(sp->ws,u);
	return p;
}

#define VMOD_HMAC_FOO(hash,hashup) \
const char * \
vmod_hmac_ ## hash(struct sess *sp, const char *key, const char *msg) \
{ \
	assert(msg); \
	assert(key); \
	return vmod_hmac_generic(sp, MHASH_ ## hashup, key, msg); \
}


VMOD_HMAC_FOO(sha256,SHA256)
VMOD_HMAC_FOO(sha1,SHA1)
VMOD_HMAC_FOO(md5,MD5)
