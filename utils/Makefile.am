EXTRA_DIST = makedist.sh
SUBDIRS=game

AM_CFLAGS = -std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions

CLEANFILES=@EXTRA_CLEAN@

install-data-local:
	sh makedist.sh $(prefix)/$(subdir)
