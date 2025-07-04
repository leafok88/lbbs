ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = README.md LICENSE conf data
SUBDIRS = src utils

install-data-local:
	install -d $(prefix)/conf $(prefix)/data $(prefix)/log $(prefix)/var $(prefix)/var/articles $(prefix)/var/chicken $(prefix)/var/gen_ex
	install -t $(prefix)/conf $(srcdir)/conf/*
	install -t $(prefix)/data $(srcdir)/data/*
	find $(prefix)/ -type d -exec chmod 750 {} \;

uninstall-local:
	cd $(srcdir); find conf -type f -exec rm -f $(prefix)/{} \;
	cd $(srcdir); find data -type f -exec rm -f $(prefix)/{} \;
