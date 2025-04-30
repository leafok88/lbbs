bin_PROGRAMS = bbsd
EXTRA_PROGRAMS =

bbsd_SOURCES = bbs.c bbs_cmd.c bbs_main.c common.c database.c fork.c init.c \
			   io.c log.c login.c menu.c main.c menu_proc.c netio.c net_server.c \
			   reg_ex.c register.c screen.c tcplib.c user_priv.c \
			   welcome.c \
			   bbs.h bbs_cmd.h common.h database.h io.h menu.h menu_proc.h tcplib.h

bbsd_LDFLAGS=-export-dynamic

lib_LTLIBRARIES = libbbs.la

libbbs_la_SOURCES = bbs_net.c bbs.h common.h io.h menu.h tcplib.h

AM_CPPFLAGS = -I../include -I/usr/include/mysql
LIBS = -L../lib -L/usr/lib/mysql -L/usr/lib64/mysql -lmysqlclient -lz -ldl

libdir=${prefix}/lib

CLEANFILES=@EXTRA_CLEAN@
