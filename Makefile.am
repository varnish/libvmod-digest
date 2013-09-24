ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

dist_man_MANS = vmod_digest.3
MAINTAINERCLEANFILES = $(dist_man_MANS)
EXTRA_DIST = README.rst

vmod_digest.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

snappy:
	./configure --build=x86_64-linux-gnu --prefix=/usr \
	--includedir=${prefix}/include --mandir=${prefix}/share/man \
	--infodir=${prefix}/share/info --sysconfdir=/etc --localstatedir=/var \
	--libexecdir=${prefix}/lib/libvmod-digest --disable-dependency-tracking \
	VMODDIR=/usr/lib/varnish/vmods \
	VARNISHSRC=~/working/varnish/varnish-3.0.4
	make
	sudo make install DESTDIR=$(HOME)/installdir/libvmod-digest-1.0
	sudo fpm -s dir -t deb \
		-n libvmod-digest -v 1.0 --iteration snappytv \
		-p libvmod-digest-VERSION~ITERATION_ARCH.deb \
		-d "varnish (3.0.4)" -d "libmhash2" \
		-m "Ramin Khatibi <ops@snappytv.com>" \
		--url "https://github.com/snappytv/libvmod-digest" \
		--description "Varnish vmod for Akamai Geo" \
		-C $(HOME)/installdir/libvmod-digest-1.0 .
