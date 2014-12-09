AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_digest.la

libvmod_digest_la_LDFLAGS = -module -export-dynamic -avoid-version -shared -lmhash

libvmod_digest_la_SOURCES = \
	vcc_if.c \
	vcc_if.h \
	vmod_digest.c

vcc_if.c vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_digest.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_digest.vcc

VMOD_TESTS = $(top_srcdir)/src/tests/*.vtc
.PHONY: $(VMOD_TESTS)

$(top_srcdir)/src/tests/*.vtc:
	@VARNISHTEST@ -Dvarnishd=@VARNISHD@ -Dvmod_topbuild=$(abs_top_builddir) $@

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_digest.vcc \
	$(VMOD_TESTS)

CLEANFILES = $(builddir)/vcc_if.c $(builddir)/vcc_if.h \
	$(builddir)/vmod_digest.man.rst \
	$(builddir)/vmod_digest.rst
