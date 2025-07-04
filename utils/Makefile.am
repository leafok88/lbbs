EXTRA_DIST = bin conf lib
SUBDIRS=game

AM_CFLAGS = -std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions

install-exec-local:
	install -d $(prefix)/$(subdir)/bin $(prefix)/$(subdir)/lib
	install -t $(prefix)/$(subdir)/bin $(srcdir)/bin/*
	install -t $(prefix)/$(subdir)/lib $(srcdir)/lib/*

install-data-local:
	install -d $(prefix)/$(subdir)/conf
	install -t $(prefix)/$(subdir)/conf $(srcdir)/conf/*

uninstall-local:
	cd $(srcdir); find bin -type f -exec rm -f $(prefix)/$(subdir)/{} \;
	cd $(srcdir); find conf -type f -exec rm -f $(prefix)/$(subdir)/{} \;
	cd $(srcdir); find lib -type f -exec rm -f $(prefix)/$(subdir)/{} \;
