/*-
 * Copyright (c) 2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Kristian Lyngstøl <kristian@varnish-cache.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Digest vmod for Varnish, using libmhash.
 * See README.rst for usage.
 */

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
#include "cache/cache.h"
#include "vcc_if.h"
#include "config.h"

enum alphabets {
	BASE64 = 0,
	BASE64URL = 1,
	BASE64URLNOPAD = 2,
	N_ALPHA
};

static struct e_alphabet {
	char *b64;
	char i64[256];
	char padding;
} alphabet[N_ALPHA];

/*
 * Initializes the reverse lookup-table for the relevant base-N alphabet.
 */
static void
vmod_digest_alpha_init(struct e_alphabet *alpha)
{
	int i;
	const char *p;

	for (i = 0; i < 256; i++)
		alpha->i64[i] = -1;
	for (p = alpha->b64, i = 0; *p; p++, i++)
		alpha->i64[(int)*p] = (char)i;
	if (alpha->padding)
		alpha->i64[(int)alpha->padding] = 0;
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
	vmod_digest_alpha_init(&alphabet[BASE64]);
	vmod_digest_alpha_init(&alphabet[BASE64URL]);
	vmod_digest_alpha_init(&alphabet[BASE64URLNOPAD]);
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

	if (inlen == 0) {
		*out = '\0';
		return (1);
	}

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

VCL_STRING
vmod_hmac_generic(const struct vrt_ctx *ctx, hashid hash, const char *key, const char *msg)
{
	size_t blocksize = mhash_get_block_size(hash);
	unsigned char mac[blocksize];
	unsigned char *hexenc;
	unsigned char *hexptr;
	int j;
	MHASH td;

	assert(msg);
	assert(key);
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

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
	hexenc = (void *)WS_Alloc(ctx->ws, 2*blocksize+3); // 0x, '\0' + 2 per input
	if (hexenc == NULL)
		return NULL;
	hexptr = hexenc;
	sprintf((char*)hexptr,"0x");
	hexptr+=2;
	for (j = 0; j < blocksize; j++) {
		sprintf((char*)hexptr,"%.2x", mac[j]);
		hexptr+=2;
		assert((hexptr-hexenc)<(2*blocksize + 3));
	}
	*hexptr = '\0';
	return (const char *)hexenc;
}

VCL_STRING
vmod_base64_generic(const struct vrt_ctx *ctx, enum alphabets a, const char *msg)
{
	char *p;
	int u;

	assert(msg);
	assert(a<N_ALPHA);

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

	u = WS_Reserve(ctx->ws,0);
	p = ctx->ws->f;
	u = base64_encode(&alphabet[a],msg,strlen(msg),p,u);
	if (u < 0) {
		WS_Release(ctx->ws,0);
		return NULL;
	}
	WS_Release(ctx->ws,u);
	return p;
}

VCL_STRING
vmod_base64_decode_generic(const struct vrt_ctx *ctx, enum alphabets a, const char *msg)
{
	char *p;
	int u;

	assert(msg);
	assert(a<N_ALPHA);

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

	u = WS_Reserve(ctx->ws,0);
	p = ctx->ws->f;
	u = base64_decode(&alphabet[a], p,u,msg);
	if (u < 0) {
		WS_Release(ctx->ws,0);
		return NULL;
	}
	WS_Release(ctx->ws,u);
	return p;
}

VCL_STRING
vmod_hash_generic(const struct vrt_ctx *ctx, hashid hash, const char *msg)
{
	MHASH td;
	unsigned char h[mhash_get_block_size(hash)];
	int i;
	char *p;
	char *ptmp;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	td = mhash_init(hash);
	mhash(td, msg, strlen(msg));
	mhash_deinit(td, h);
	p = WS_Alloc(ctx->ws,mhash_get_block_size(hash)*2 + 1);
	ptmp = p;
	for (i = 0; i<mhash_get_block_size(hash);i++) {
		sprintf(ptmp,"%.2x",h[i]);
		ptmp+=2;
	}
	return p;
}

#define VMOD_HASH_FOO(low, high) \
VCL_STRING __match_proto__ () \
vmod_hash_ ## low (const struct vrt_ctx *ctx, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return vmod_hash_generic(ctx, MHASH_ ## high, msg); \
}

VMOD_HASH_FOO(sha1,SHA1)
VMOD_HASH_FOO(sha224,SHA224)
VMOD_HASH_FOO(sha256,SHA256)
VMOD_HASH_FOO(sha384,SHA384)
VMOD_HASH_FOO(sha512,SHA512)
VMOD_HASH_FOO(gost,GOST)
VMOD_HASH_FOO(md2,MD2)
VMOD_HASH_FOO(md4,MD4)
VMOD_HASH_FOO(md5,MD5)
VMOD_HASH_FOO(crc32,CRC32)
VMOD_HASH_FOO(crc32b,CRC32B)
VMOD_HASH_FOO(adler32,ADLER32)
VMOD_HASH_FOO(haval128,HAVAL128)
VMOD_HASH_FOO(haval160,HAVAL160)
VMOD_HASH_FOO(haval192,HAVAL192)
VMOD_HASH_FOO(haval224,HAVAL224)
VMOD_HASH_FOO(haval256,HAVAL256)
VMOD_HASH_FOO(ripemd128,RIPEMD128)
VMOD_HASH_FOO(ripemd160,RIPEMD160)
VMOD_HASH_FOO(ripemd256,RIPEMD256)
VMOD_HASH_FOO(ripemd320,RIPEMD320)
VMOD_HASH_FOO(tiger,TIGER)
VMOD_HASH_FOO(tiger128,TIGER128)
VMOD_HASH_FOO(tiger160,TIGER160)
VMOD_HASH_FOO(snefru128,SNEFRU128)
VMOD_HASH_FOO(snefru256,SNEFRU256)
VMOD_HASH_FOO(whirlpool,WHIRLPOOL)

#define VMOD_ENCODE_FOO(codec_low,codec_big) \
VCL_STRING __match_proto__ () \
vmod_ ## codec_low (const struct vrt_ctx *ctx, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return vmod_base64_generic(ctx,codec_big,msg); \
} \
\
const char * __match_proto__ () \
vmod_ ## codec_low ## _decode (const struct vrt_ctx *ctx, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return vmod_base64_decode_generic(ctx,codec_big,msg); \
}

VMOD_ENCODE_FOO(base64,BASE64)
VMOD_ENCODE_FOO(base64url,BASE64URL)
VMOD_ENCODE_FOO(base64url_nopad,BASE64URLNOPAD)

/*
 * XXX: We assume it's better to return a NULL-string if no key is present,
 * XXX: to avoid having bugs that are "invisible" due to an actual hash
 * XXX: being made. For the content, blank data is valid.
 */
#define VMOD_HMAC_FOO(hash,hashup) \
VCL_STRING \
vmod_hmac_ ## hash(const struct vrt_ctx *ctx, const char *key, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	if (key == NULL) \
		return NULL; \
	return vmod_hmac_generic(ctx, MHASH_ ## hashup, key, msg); \
}


VMOD_HMAC_FOO(sha256,SHA256)
VMOD_HMAC_FOO(sha1,SHA1)
VMOD_HMAC_FOO(md5,MD5)


VCL_STRING __match_proto__()
vmod_version(const struct vrt_ctx *ctx  __attribute__((unused)))
{
	return VERSION;
}
