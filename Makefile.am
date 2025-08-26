ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = README.md INSTALL.md ChangeLog.md LICENSE conf data
SUBDIRS = src utils

install-data-local:
	install -d $(prefix)/conf $(prefix)/data $(prefix)/log $(prefix)/var $(prefix)/var/articles $(prefix)/var/chicken $(prefix)/var/gen_ex
	cd $(srcdir)/conf; find . -maxdepth 1 -type f -exec install -t $(prefix)/conf {} \;
	cd $(srcdir)/data; find . -maxdepth 1 -type f -exec install -t $(prefix)/data {} \;
	find $(prefix)/ -type d -exec chmod 750 {} \;

uninstall-local:
	cd $(srcdir)/conf; find . -type f -exec rm -f $(prefix)/conf/{} \;
	cd $(srcdir)/data; find . -type f -exec rm -f $(prefix)/data/{} \;
