AM_CFLAGS = $(VCACHEAPI_CFLAGS)
AM_LDFLAGS = $(VCACHEAPI_LIBS) $(VMOD_LDFLAGS)

vmod_LTLIBRARIES = libvmod_digest.la

libvmod_digest_la_LDFLAGS = $(VMOD_LDFLAGS) -lmhash

libvmod_digest_la_SOURCES = \
	vmod_digest.c

nodist_libvmod_digest_la_SOURCES = \
	vcc_digest_if.c \
	vcc_digest_if.h

@BUILD_VMOD_DIGEST@

AM_TESTS_ENVIRONMENT = \
	PATH="$(VMOD_TEST_PATH)" \
	LD_LIBRARY_PATH="$(VCACHE_LIBRARY_PATH)"
TEST_EXTENSIONS = .vtc
VTC_LOG_COMPILER = varnishtest -v
AM_VTC_LOG_FLAGS = -Dvmod_topbuild=$(abs_top_builddir)
TESTS = @VMOD_TESTS@

EXTRA_DIST = \
	vmod_digest.vcc \
	$(TESTS)

CLEANFILES = \
	$(builddir)/vcc_digest_if.c \
	$(builddir)/vcc_digest_if.h \
	$(builddir)/vmod_digest.man.rst \
	$(builddir)/vmod_digest.rst
