bin_PROGRAMS = bbsd
EXTRA_PROGRAMS = test_trie_dict test_file_loader test_section_list test_lml test_ssh_server test_article_view_log test_memory_pool

bbsd_SOURCES = article_cache.c article_view_log.c bbs.c bbs_cmd.c bbs_main.c common.c database.c editor.c \
			   file_loader.c init.c io.c lml.c log.c login.c memory_pool.c menu.c main.c menu_proc.c net_server.c \
			   screen.c section_list.c section_list_display.c section_list_loader.c str_process.c \
			   trie_dict.c user_priv.c

bbsd_LDFLAGS=-export-dynamic

test_trie_dict_SOURCES = log.c test_trie_dict.c trie_dict.c
test_file_loader_SOURCES = file_loader.c log.c str_process.c test_file_loader.c trie_dict.c
test_section_list_SOURCES = common.c log.c section_list.c test_section_list.c trie_dict.c
test_lml_SOURCES = lml.c log.c test_lml.c
test_ssh_server_SOURCES = log.c test_ssh_server.c
test_article_view_log_SOURCES = article_view_log.c database.c log.c test_article_view_log.c
test_memory_pool_SOURCES = log.c memory_pool.c test_memory_pool.c

lib_LTLIBRARIES = libbbs.la

libbbs_la_SOURCES = bbs_net.c

AM_CFLAGS = -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions
AM_CPPFLAGS = -I../include
LIBS = -L/usr/lib64/mysql/ -lmysqlclient -lsystemd -lssh -lz -ldl

libdir = ${prefix}/lib

CLEANFILES = @EXTRA_CLEAN@ ${EXTRA_PROGRAMS}
