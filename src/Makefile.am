bin_PROGRAMS = bbsd
test_PROGRAMS = test_trie_dict
EXTRA_PROGRAMS =

testdir = ./

bbsd_SOURCES = bbs.c bbs_cmd.c bbs_main.c common.c database.c fork.c init.c \
			   io.c log.c login.c menu.c main.c menu_proc.c net_server.c \
			   register.c screen.c str_process.c trie_dict.c user_priv.c

bbsd_LDFLAGS=-export-dynamic

test_trie_dict_SOURCES = trie_dict.c test_trie_dict.c

lib_LTLIBRARIES = libbbs.la

libbbs_la_SOURCES = bbs_net.c

AM_CFLAGS = -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions
AM_CPPFLAGS = -I../include -I/usr/include/mysql
LIBS = -L/usr/lib/mysql -L/usr/lib64/mysql -lmysqlclient -lz -ldl -lsystemd

libdir = ${prefix}/lib

CLEANFILES = @EXTRA_CLEAN@
