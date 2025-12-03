ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = README.md README.zh_CN.md INSTALL.md INSTALL.zh_CN.md ChangeLog.md ChangeLog.zh_CN.md LICENSE conf data
SUBDIRS = utils src

install-data-local:
	install -d $(prefix)/conf $(prefix)/data $(prefix)/log $(prefix)/var $(prefix)/var/articles $(prefix)/var/chicken $(prefix)/var/gen_ex
	cd $(srcdir)/conf; find . -maxdepth 1 -type f -exec install -t $(prefix)/conf -m 644 -p {} \;
	cd $(srcdir)/data; find . -maxdepth 1 -type f -exec install -t $(prefix)/data -m 644 -p {} \;
	find $(prefix)/ -type d -exec chmod 750 {} \;

uninstall-local:
	cd $(srcdir)/conf; find . -maxdepth 1 -type f -exec rm -f $(prefix)/conf/{} \;
	cd $(srcdir)/data; find . -maxdepth 1 -type f -exec rm -f $(prefix)/data/{} \;
