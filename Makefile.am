ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = README.md LICENSE makedist.sh
SUBDIRS=src utils
CLEANFILES=@EXTRA_CLEAN@

install-data-local:
	sh makedist.sh $(prefix)
