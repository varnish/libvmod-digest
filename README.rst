===========
vmod_digest
===========

---------------------
Varnish Digest Module
---------------------

:Manual section: 3
:Author: Kristian Lyngstøl
:Date: 2011-09-22
:Version: 0.3

SYNOPSIS
========

::

        import digest;
        
        digest.hmac_md5(<key>,<message>);
        digest.hmac_sha1(<key>, <message>);
        digest.hmac_sha256(<key>, <message));

        digest.base64(<string>);
        digest.base64url(<string>);
        digest.base64url_nopad(<string>);
        digest.base64_decode(<string>);
        digest.base64url_decode(<string>);
        digest.base64url_nopad_decode(<string>);

        digest.version()

        digest.hash_sha1(<string>);
        digest.hash_sha224(<string>);
        digest.hash_sha256(<string>);
        digest.hash_sha384(<string>);
        digest.hash_sha512(<string>);
        digest.hash_gost(<string>);
        digest.hash_md2(<string>);
        digest.hash_md4(<string>);
        digest.hash_md5(<string>);
        digest.hash_crc32(<string>);
        digest.hash_crc32b(<string>);
        digest.hash_adler32(<string>);
        digest.hash_haval128(<string>);
        digest.hash_haval160(<string>);
        digest.hash_haval192(<string>);
        digest.hash_haval224(<string>);
        digest.hash_haval256(<string>);
        digest.hash_ripemd128(<string>);
        digest.hash_ripemd160(<string>);
        digest.hash_ripemd256(<string>);
        digest.hash_ripemd320(<string>);
        digest.hash_tiger(<string>);
        digest.hash_tiger128(<string>);
        digest.hash_tiger160(<string>);
        digest.hash_snefru128(<string>);
        digest.hash_snefru256(<string>);

DESCRIPTION
===========

Varnish Module (vmod) for computing HMAC, message digests and working with
base64.

All HMAC- and hash-functionality is provided by libmhash, while base64 is
implemented locally.

FUNCTIONS
=========

Example VCL::

	backend foo { ... };

	import digest;

	sub vcl_recv {
		if (digest.hmac_sha256("key",req.http.x-some-header) !=
			digest.hmac_sha256("key",req.http.x-some-header-signed))
		{
			return (synth(401, "Naughty user!"));
		}
	}


hmac_(hash)
-----------

Prototype
        ::

	        digest.hmac_md5(<key>,<message>);
	        digest.hmac_sha1(<key>, <message>);
	        digest.hmac_sha256(<key>, <message));
Returns
        String. Hex-encoded prepended with 0x.
Description
        All the various hmac-functions work the same, but use a different
	hash mechanism.
Example
        ::

                set resp.http.x-data-sig = 
                        digest.hmac_sha256("secretkey",resp.http.x-data);

base64, base64url, base64url_nopad
----------------------------------

Prototype
        ::

                digest.base64(<string>);
                digest.base64url(<string>);
                digest.base64url_nopad(<string>);
Returns
        String
Description
        Returns the base64-encoded version of the input-string. The
        base64url-variant uses base64 url-encoding (+/ replaced by -_) and
        the base64url_nopad does the same, but avoids adding padding. The
        latter is more commonly used, though an (allowed) exception to the
        RFC4648.
Example
        ::

                set resp.http.x-data-sig = 
                        digest.base64({"content with
                        newline in it"});

base64_hex, base64_url_hex, base64url_nopad_hex
-----------------------------------------------

Prototype
        ::

                digest.base64_hex(<string>);
                digest.base64url_hex(<string>);
                digest.base64url_nopad_hex(<string>);
Returns
        String
Description
        Returns the base64-encoded version of the hex encoded input-string. The
        input-string can start with an optional 0x. Input is hex-decoded and
        the encoding is identical to base64, base64url, and base64url_nopad.
Example
        ::

                set resp.http.x-data-sig =
                        digest.base64_hex("0xdd26bfddf122c1055d4c");

hash_(algorithm)
----------------

Prototype
        ::
        
                digest.hash_sha1(<string>);
                digest.hash_sha224(<string>);
                digest.hash_sha256(<string>);
                digest.hash_sha384(<string>);
                digest.hash_sha512(<string>);
                digest.hash_gost(<string>);
                digest.hash_md2(<string>);
                digest.hash_md4(<string>);
                digest.hash_md5(<string>);
                digest.hash_crc32(<string>);
                digest.hash_crc32b(<string>);
                digest.hash_adler32(<string>);
                digest.hash_haval128(<string>);
                digest.hash_haval160(<string>);
                digest.hash_haval192(<string>);
                digest.hash_haval224(<string>);
                digest.hash_haval256(<string>);
                digest.hash_ripemd128(<string>);
                digest.hash_ripemd160(<string>);
                digest.hash_ripemd256(<string>);
                digest.hash_ripemd320(<string>);
                digest.hash_tiger(<string>);
                digest.hash_tiger128(<string>);
                digest.hash_tiger160(<string>);
                digest.hash_snefru128(<string>);
                digest.hash_snefru256(<string>);
Returns
        String
Description
        Computes the digest/hash of the supplied, using the specified hash
        algorithm. If in doubt as to which to pick, use SHA256. For
        detailed discussions, see The Internet.
Example
        ::
                
                set resp.http.x-data-md5 = 
                        digest.hash_md5(resp.http.x-data);

base64_decode, base64url_decode, base64url_nopad_decode
-------------------------------------------------------

Prototype
        ::
        
                digest.base64_decode(<string>);
                digest.base64url_decode(<string>);
                digest.base64url_nopad_decode(<string>);
Returns
        String
Description
        Decodes the bas64 and base64url-encoded strings. All functions
        treat padding the same, meaning base64url_decode and
        base64url_nopad_decode are identical, but available for consistency
        and practicality.
Example
        ::
                synthetic(digest.base64_decode(req.http.x-parrot));

version
-------

Prototype
        ::

                digest.version()
Returns
        string
Description
        Returns the string constant version-number of the digest vmod.
Example
        ::
                
                set resp.http.X-digest-version = digest.version();


INSTALLATION
============

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the ``varnishtest`` tool.

Building requires the Varnish header files and uses pkg-config to find
the necessary paths.

Usage::

 ./autogen.sh
 ./configure

If you have installed Varnish to a non-standard directory, call
``autogen.sh`` and ``configure`` with ``PKG_CONFIG_PATH`` pointing to
the appropriate path. For example, when varnishd configure was called
with ``--prefix=$PREFIX``, use

 PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
 export PKG_CONFIG_PATH

Make targets:

* make - builds the vmod.
* make install - installs your vmod.
* make check - runs the unit tests in ``src/tests/*.vtc``
* make distcheck - run check and prepare a tarball of the vmod.


ACKNOWLEDGEMENTS
================

Author: Kristian Lyngstøl <kristian@varnish-software.com>, Varnish Software AS

This Vmod was written for Media Norge, Schibsted and others.

The bulk of the functionality is acquired through libmhash

Bug reports by: Magnus Hagander

HISTORY
=======

Version 0.1: Initial version, mostly feature-complete

Version 0.2: Mainly build-related cleanups, no feature-changes

Version 0.3: Handle empty/NULL strings for hashes and keys.

BUGS
====

No bugs at all!

If the key is NULL for hmac-functions, the function will fail and return
NULL itself, and do no hmac-computation at all. This should be used as an
indication of some greater flaw in your software/VCL. (I.e.: Your key
should be under your control, not user-supplied without verification).

The `base64url_nopad_decode()` and `base64url_decode()` functions do not
differ much. The exception is that nopad_decode() does not know about
padding at all, and might get confused if the input actually is padded.

SEE ALSO
========

* libmhash
* varnishd(1)
* vcl(7)
* https://github.com/varnish/libvmod-digest

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-digest project. See LICENSE for details.

* Copyright (c) 2011 Varnish Software
