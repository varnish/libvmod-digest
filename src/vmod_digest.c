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

#include "config.h"

#include <stdbool.h>
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

#include <cache/cache.h>
#include <vcl.h>

#ifndef VRT_H_INCLUDED
#  include <vrt.h>
#endif

#ifndef VDEF_H_INCLUDED
#  include <vdef.h>
#endif

#include "vcc_if.h"
/* Varnish < 6.2 compat */
#ifndef VPFX
#define VPFX(a) vmod_ ## a
#define VARGS(a) vmod_ ## a ## _arg
#define VENUM(a) vmod_enum_ ## a
#define INIT_FUNCTION init_function
#else
#define INIT_FUNCTION VPFX(init_function)
#endif

#ifndef MIN
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif

#ifndef VRT_CTX
#define VRT_CTX		const struct vrt_ctx *ctx
#endif

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
VPFX(digest_alpha_init)(struct e_alphabet *alpha)
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
INIT_FUNCTION(VRT_CTX, struct VPFX(priv) *priv, enum vcl_event_e e)
{
	(void)ctx;
	(void)priv;

	if (e != VCL_EVENT_LOAD)
		return (0);

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
	VPFX(digest_alpha_init)(&alphabet[BASE64]);
	VPFX(digest_alpha_init)(&alphabet[BASE64URL]);
	VPFX(digest_alpha_init)(&alphabet[BASE64URLNOPAD]);
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
 * Convert a hex character into an int
 */
static unsigned char
char_to_int (char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 87;
	else if (c >= 'A' && c <= 'F')
		return c - 55;
	else
		return 0;
}

/*
 * Convert a hex value into an 8bit int
 */
static unsigned char
hex_to_int(const char *in, size_t inlen)
{
	unsigned char value = 0;

	assert(inlen >= 2);

	value = char_to_int(in[0]) << 4;
	value += char_to_int(in[1]);

	return value;
}

/*
 * Base64-encode *in (size: inlen) into *out, max outlen bytes. If there is
 * insufficient space, it will bail out and return -1. Otherwise, it will
 * null-terminate and return the used space.
 * The alphabet `a` defines... the alphabet. Padding is optional.
 * Inspired heavily by gnulib/Simon Josefsson (as referenced in RFC4648)
 */
static size_t
base64_encode(struct e_alphabet *alpha, const char *in,
    size_t inlen, int is_hex, char *out, size_t outlen)
{
	size_t out_used = 0;

	/*
	 * If reading a hex string, if "0x" is present, strip. When no further
	 * characters follow, we return an empty output string.
	 */
	if (is_hex && inlen > 2 && in[0] == '0' && in[1] == 'x') {
		in += 2;
		inlen -= 2;
	}

	/*
	 * B64 requires 4*ceil(n/3) bytes of space + 1 nul terminator
	 * byte to generate output for a given input length n. When is_hex is
	 * set, each character of inlen represents half a byte, hence the
	 * division by 6.
	 */
	if ((!is_hex && outlen < 4 * (inlen + 2 / 3) + 1) ||
	    ( is_hex && outlen < 4 * (inlen + 5 / 6) + 1))
		return -1;

	while ((!is_hex && inlen) || (is_hex && inlen >= 2)) {
		unsigned char tmp[3] = {0, 0, 0};
		unsigned char idx;
		int min_avail = is_hex ? MIN(inlen, 6) : MIN(inlen, 3);
		int nread = 0;
		int off = 0;

		if (is_hex) {
			while (min_avail >= 2) {
				tmp[off++] = hex_to_int(in, inlen);
				in += 2;
				inlen -= 2;
				nread++;
				min_avail -= 2;
			}
		} else {
			memcpy(tmp, in, min_avail);
			in += min_avail;
			inlen -= min_avail;
			nread = min_avail;
		}

		*out++ = alpha->b64[(tmp[0] >> 2) & 0x3f];

		idx = (tmp[0] << 4);
		if (nread > 1)
			idx += (tmp[1] >> 4);
		idx &= 0x3f;
		*out++ = alpha->b64[idx];

		if (nread > 1) {
			idx = (tmp[1] << 2);
			if (nread > 2)
				idx += tmp[2] >> 6;
			idx &= 0x3f;

			*out++ = alpha->b64[idx];
		} else if (alpha->padding)
			*out++ = alpha->padding;

		if (nread > 2)
			*out++ = alpha->b64[tmp[2] & 0x3f];
		else if (alpha->padding)
			*out++ = alpha->padding;

		if (alpha->padding)
			out_used += 4;
		else
			out_used += 2 + (nread - 1);
	}

	*out = '\0';

	return out_used + 1;
}

VCL_STRING
VPFX(hmac_generic)(VRT_CTX, hashid hash, const char *key, const char *msg)
{
	size_t blocksize = mhash_get_block_size(hash);
	unsigned char mac[blocksize];
	unsigned char *hexenc;
	unsigned char *hexptr;
	size_t j;
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
		assert((hexptr-hexenc)<(2*(long)blocksize + 3));
	}
	*hexptr = '\0';
	return (const char *)hexenc;
}

VCL_STRING
VPFX(base64_generic)(VRT_CTX, enum alphabets a, const char *msg, int is_hex)
{
	char *p;
	int u;

	assert(msg);
	assert(a<N_ALPHA);

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

	u = WS_Reserve(ctx->ws,0);
	p = ctx->ws->f;
	u = base64_encode(&alphabet[a],msg,strlen(msg),is_hex,p,u);
	if (u < 0) {
		WS_Release(ctx->ws,0);
		return NULL;
	}
	WS_Release(ctx->ws,u);
	return p;
}

VCL_STRING
VPFX(base64_decode_generic)(VRT_CTX, enum alphabets a, const char *msg)
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
VPFX(hash_generic)(VRT_CTX, hashid hash, const char *msg)
{
	MHASH td;
	unsigned char h[mhash_get_block_size(hash)];
	unsigned int i;
	char *p;
	char *ptmp;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	td = mhash_init(hash);
	mhash(td, msg, strlen(msg));
	mhash_deinit(td, h);
	p = WS_Alloc(ctx->ws,mhash_get_block_size(hash)*2 + 1);
	AN(p);
	ptmp = p;
	for (i = 0; i<mhash_get_block_size(hash);i++) {
		sprintf(ptmp,"%.2x",h[i]);
		ptmp+=2;
	}
	return p;
}

#define VMOD_HASH_FOO(low, high) \
VCL_STRING \
vmod_hash_ ## low (VRT_CTX, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return VPFX(hash_generic)(ctx, MHASH_ ## high, msg); \
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
VCL_STRING \
vmod_ ## codec_low (VRT_CTX, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return VPFX(base64_generic)(ctx,codec_big,msg, 0); \
} \
\
VCL_STRING \
vmod_ ## codec_low ## _hex (VRT_CTX, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return VPFX(base64_generic)(ctx,codec_big,msg, 1); \
} \
\
const char * \
vmod_ ## codec_low ## _decode (VRT_CTX, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	return VPFX(base64_decode_generic)(ctx,codec_big,msg); \
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
vmod_hmac_ ## hash(VRT_CTX, const char *key, const char *msg) \
{ \
	if (msg == NULL) \
		msg = ""; \
	if (key == NULL) \
		return NULL; \
	return VPFX(hmac_generic)(ctx, MHASH_ ## hashup, key, msg); \
}


VMOD_HMAC_FOO(sha256,SHA256)
VMOD_HMAC_FOO(sha1,SHA1)
VMOD_HMAC_FOO(md5,MD5)


VCL_STRING
VPFX(version)(VRT_CTX)
{
	(void)ctx;
	return VERSION;
}
