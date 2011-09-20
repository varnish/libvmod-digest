#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * mhash.h has a habit of pulling in assert(). Let's hope it's a define,
 * and that we can undef it, since Varnish has a better one.
 */
#include <mhash.h>
#ifdef assert
#	undef assert
#endif

#include "vrt.h"
#include "bin/varnishd/cache.h"
#include "vsha256.h"
#include "vcc_if.h"
#include "config.h"


enum alphabets {
	BASE64 = 0,
	BASE64URL = 1,
	BASE64URLNOPAD = 2,
	N_ALPHA
};

struct e_alphabet {
	char *b64;
	char i64[256];
	char padding;
} alphabet[N_ALPHA];


static void
VB64_init(struct e_alphabet *alpha)
{
	int i;
	const char *p;

	for (i = 0; i < 256; i++)
		alpha->i64[i] = -1;
	for (p = alpha->b64, i = 0; *p; p++, i++)
		alpha->i64[(int)*p] = (char)i;
	if (alpha->padding)
		alpha->i64[alpha->padding] = 0;
}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
    	alphabet[BASE64].b64 =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		"ghijklmnopqrstuvwxyz0123456789+/";
	alphabet[BASE64].padding = '=';
	alphabet[BASE64URL].b64 =
		 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		 "ghijklmnopqrstuvwxyz0123456789-_";
	alphabet[BASE64URL].padding = '=';
	alphabet[BASE64URLNOPAD].b64 =
		 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		 "ghijklmnopqrstuvwxyz0123456789-_";
	alphabet[BASE64URLNOPAD].padding = 0;
	VB64_init(&alphabet[BASE64]);
	VB64_init(&alphabet[BASE64URL]);
	VB64_init(&alphabet[BASE64URLNOPAD]);
	return (0);
}

/*
 * Decodes the string s into the buffer d (size dlen), using the alphabet
 * specified.
 *
 * Modified slightly from varnishncsa's decoder. Mainly because the
 * input-length is known, so padding is optional (this is per the RFC and
 * allows this code to be used regardless of whether padding is present).
 * Also returns the length of data when it succeeds.
 */
static int
base64_decode(struct e_alphabet *alpha, char *d, unsigned dlen, const char *s)
{
	unsigned u, v, l;
	int i;

	u = 0;
	l = 0;
	while (*s) {
		for (v = 0; v < 4; v++) {
			if (*s)
				i = alpha->i64[(int)*s++];
			else
				i = 0;
			if (i < 0)
				return (-1);
			u <<= 6;
			u |= i;
		}
		for (v = 0; v < 3; v++) {
			if (l >= dlen - 1)
				return (-1);
			*d = (u >> 16) & 0xff;
			u <<= 8;
			l++;
			d++;
		}
		if (!*s)
			break;
	}
	*d = '\0';
	l++;
	return (l);
}

/*
 * Base64-encode *in (size: inlen) into *out, max outlen bytes. If there is
 * insufficient space, it will bail out and return -1. Otherwise, it will
 * null-terminate and return the used space.
 * The alphabet `a` defines... the alphabet. Padding is optional.
 * Inspired heavily by gnulib/Simon Josefsson (as referenced in RFC4648)
 *
 * XXX: tmp[] and idx are used to ensure the reader (and author) retains
 * XXX: a limited amount of sanity. They are strictly speaking not
 * XXX: necessary, if you don't mind going crazy.
 *
 * FIXME: outlenorig is silly. Flip the logic.
 */
static size_t
base64_encode (struct e_alphabet *alpha, const char *in,
		size_t inlen, char *out, size_t outlen)
{
	size_t outlenorig = outlen;
	unsigned char tmp[3], idx;

	if (outlen<4)
		return -1;

	while (1) {
		assert(inlen);
		assert(outlen>3);

		tmp[0] = (unsigned char) in[0];
		tmp[1] = (unsigned char) in[1];
		tmp[2] = (unsigned char) in[2];

		*out++ = alpha->b64[(tmp[0] >> 2) & 0x3f];

		idx = (tmp[0] << 4);
		if (inlen>1)
			idx += (tmp[1] >> 4);
		idx &= 0x3f;
		*out++ = alpha->b64[idx];
			
		if (inlen>1) {
			idx = (tmp[1] << 2);
			if (inlen>2)
				idx += tmp[2] >> 6;
			idx &= 0x3f;

			*out++ = alpha->b64[idx];
		} else {
			if (alpha->padding)
				*out++ = alpha->padding;
		}

		if (inlen>2) {
			*out++ = alpha->b64[tmp[2] & 0x3f];
		} else {
			if (alpha->padding)
				*out++ = alpha->padding;
		}

		/*
		 * XXX: Only consume 4 bytes, but since we need a fifth for
		 * XXX: NULL later on, we might as well test here.
		 */
		if (outlen<5)
			return -1;

		outlen -= 4;

		if (inlen<4)
			break;
		
		inlen -= 3;
		in += 3;
	}

	assert(outlen);
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
	unsigned char *hexenc;
	unsigned char *hexptr;
	int j;
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
	hexenc = WS_Alloc(sp->ws, 2*blocksize+3); // 0x, '\0' + 2 per input
	if (hexenc == NULL)
		return NULL;
	hexptr = hexenc;
	sprintf(hexptr,"0x");
	hexptr+=2;
	for (j = 0; j < blocksize; j++) {
		sprintf(hexptr,"%.2x", mac[j]);
		hexptr+=2;
		assert((hexptr-hexenc)<(2*blocksize + 3));
	}
	*hexptr = '\0';
	return hexenc;
}

const char *
vmod_base64_generic(struct sess *sp, enum alphabets a, const char *msg)
{
	char *p;
	int u;
	/*
	 * Base64 encode on the workspace (since it's returned).
	 */
	u = WS_Reserve(sp->ws,0);
	p = sp->ws->f;
	u = base64_encode(&alphabet[a],msg,strlen(msg),p,u);
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

const char *
vmod_base64_decode_generic(struct sess *sp, enum alphabets a, const char *msg)
{
	char *p;
	int u;
	/*
	 * Base64 encode on the workspace (since it's returned).
	 */
	u = WS_Reserve(sp->ws,0);
	p = sp->ws->f;
	u = base64_decode(&alphabet[a], p,u,msg);
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

#define VMOD_ENCODE_FOO(codec_low,codec_big) \
const char * __match_proto__ () \
vmod_ ## codec_low (struct sess *sp, const char *msg) \
{ \
	assert(msg); \
	return vmod_base64_generic(sp,codec_big,msg); \
} \
\
const char * __match_proto__ () \
vmod_ ## codec_low ## _decode (struct sess *sp, const char *msg) \
{ \
	assert(msg); \
	return vmod_base64_decode_generic(sp,codec_big,msg); \
}

VMOD_ENCODE_FOO(base64,BASE64)
VMOD_ENCODE_FOO(base64url,BASE64URL)
VMOD_ENCODE_FOO(base64url_nopad,BASE64URLNOPAD)

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



const char * __match_proto__()
vmod_version(struct sess *sp __attribute__((unused)))
{
	return VERSION;
}

