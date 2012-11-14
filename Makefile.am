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
