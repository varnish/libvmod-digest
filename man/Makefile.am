#

dist_man_MANS = vmod_digest.3
MAINTAINERCLEANFILES = $(dist_man_MANS)

vmod_digest.3: vmod_digest.rst
if HAVE_RST2MAN
	${RST2MAN} vmod_digest.rst $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif
