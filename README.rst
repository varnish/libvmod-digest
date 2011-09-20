===========
vmod_digest
===========

---------------------
Varnish Digest Module
---------------------

:Manual section: 3
:Author: Kristian Lyngstøl
:Date: 2011-09-19
:Version: 0.1

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

DESCRIPTION
===========

Varnish Module (vmod) for computing HMAC and message digests.

FUNCTIONS
=========

Example VCL::

	backend foo { ... };

	import digest;

	sub vcl_recv {
		if (digest.hmac_sha256("key",req.http.x-some-header) !=
			digest.hmac_sha256("key",req.http.x-some-header-signed))
		{
			error 401 "Naughty user!";
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
        String
Description
        All the various hmac-functions work the same, but use a different
	hash mechanism.
Example
        ``set resp.http.x-data-sig = digest.hmac_sha256("secretkey",resp.http.x-data)``

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
        ``set resp.http.x-data-sig = digest.base64(digest.hmac_sha256("secret",resp.http.x-data));``

base64_decode, base64url_decode, base64url_nopad_decode
-------------------------------------------------------

Prototype
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
        if (digest.base64_decode(digest.hmac_sha256("secret",req.http.x-data)) == req.http.x-data-sig) {
                ...
        }


version
-------

Prototype
        digest.version()
Returns
        string
Description
        Returns the string constant version-number of the digest vmod.
Example
        ``set resp.http.X-digest-version = digest.version();``


INSTALLATION
============

Installation requires the Varnish source tree (only the source matching the
binary installation).

1. `./autogen.sh`  (for git-installation)
2. `./configure VARNISHSRC=/path/to/your/varnish/source/varnish-cache`
3. `make`
4. `make install` (may require root: sudo make install)
5. `make check` (Optional for regression tests)

VARNISHSRCDIR is the directory of the Varnish source tree for which to
compile your vmod. Both the VARNISHSRCDIR and VARNISHSRCDIR/include
will be added to the include search paths for your module.

Optionally you can also set the vmod install dir by adding VMODDIR=DIR
(defaults to the pkg-config discovered directory from your Varnish
installation).


ACKNOWLEDGEMENTS
================

This Vmod was written for Media Norge, Schibsted and others.

Author: Kristian Lyngstøl <kristian@varnish-software.com>, Varnish Software AS
Skeleton by Martin Blix Grydeland <martin@varnish-software.com>, vmods are
part of Varnish Cache 3.0 and beyond.

HISTORY
=======

Version 0.1: Initial version, somewhat ambiguous where it starts and ends.

BUGS
====

No actual digest-functions are exposed yet.

SEE ALSO
========

* varnishd(1)
* vcl(7)
* https://github.com/varnish/libvmod-digest

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-digest project. See LICENSE for details.

* Copyright (c) 2011 Varnish Software
