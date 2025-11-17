bin_PROGRAMS = bbsd
test_PROGRAMS = test_trie_dict test_file_loader test_section_list test_lml test_ssh_server test_article_favor test_article_view_log \
			   test_memory_pool test_bbs test_bwf test_hash_dict

testdir = ${prefix}/test

bbsd_SOURCES = ${srcdir}/../include/*.h article_cache.c article_del.c article_favor.c article_favor_display.c article_op.c \
			   article_post.c article_view_log.c bbs.c bbs_cmd.c bbs_main.c bwf.c common.c data_file_mgr.c database.c \
			   editor.c file_loader.c hash_dict.c init.c io.c ip_mask.c lml.c log.c login.c memory_pool.c menu.c main.c \
			   menu_proc.c net_server.c screen.c section_list.c section_list_display.c section_list_loader.c str_process.c \
			   trie_dict.c user_list.c user_info_display.c user_info_update.c user_list_display.c user_priv.c user_stat.c

bbsd_LDFLAGS=-export-dynamic

test_trie_dict_SOURCES = log.c test_trie_dict.c trie_dict.c
test_file_loader_SOURCES = file_loader.c log.c str_process.c test_file_loader.c trie_dict.c
test_section_list_SOURCES = bbs.c common.c database.c log.c section_list.c test_section_list.c trie_dict.c user_list.c user_priv.c user_stat.c
test_lml_SOURCES = lml.c log.c str_process.c test_lml.c
test_ssh_server_SOURCES = log.c test_ssh_server.c
test_article_favor_SOURCES = article_favor.c bbs.c common.c database.c log.c section_list.c test_article_favor.c trie_dict.c \
							 user_list.c user_priv.c user_stat.c
test_article_view_log_SOURCES = article_view_log.c database.c log.c test_article_view_log.c
test_memory_pool_SOURCES = log.c memory_pool.c test_memory_pool.c
test_bbs_SOURCES = bbs.c log.c test_bbs.c user_priv.c
test_bwf_SOURCES = bwf.c log.c test_bwf.c
test_hash_dict_SOURCES = hash_dict.c log.c memory_pool.c test_hash_dict.c

lib_LTLIBRARIES = libbbs.la

libbbs_la_SOURCES = bbs_net.c

AM_CFLAGS = -I${srcdir}/../include ${LIBSSH_CFLAGS} ${MYSQLCLIENT_CFLAGS} ${SYSTEMD_CFLAGS} ${LIBPCRE2_CFLAGS} ${ICONV_CFLAGS} \
			-std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wpedantic -Wformat-security -Wconversion \
			-Wdeprecated -finline-functions
LIBS = ${LIBSSH_LIBS} ${MYSQLCLIENT_LIBS} ${SYSTEMD_LIBS} ${LIBPCRE2_LIBS} ${ICONV_LIBS} -ldl

libdir = ${prefix}/lib
