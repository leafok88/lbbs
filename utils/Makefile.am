EXTRA_DIST = bin conf lib
SUBDIRS=game

AM_CFLAGS = -std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions

install-exec-local:
	install -d $(prefix)/$(subdir)/bin $(prefix)/$(subdir)/lib
	cd $(srcdir)/bin; find . -maxdepth 1 -type f -exec install -t $(prefix)/$(subdir)/bin {} \;
	cd $(srcdir)/lib; find . -maxdepth 1 -type f -exec install -t $(prefix)/$(subdir)/lib {} \;

install-data-local:
	install -d $(prefix)/$(subdir)/conf
	cd $(srcdir)/conf; find . -maxdepth 1 -type f -exec install -t $(prefix)/$(subdir)/conf {} \;

uninstall-local:
	cd $(srcdir)/bin; find . -type f -exec rm -f $(prefix)/$(subdir)/bin/{} \;
	cd $(srcdir)/conf; find . -type f -exec rm -f $(prefix)/$(subdir)/conf/{} \;
	cd $(srcdir)/lib; find . -type f -exec rm -f $(prefix)/$(subdir)/lib/{} \;
