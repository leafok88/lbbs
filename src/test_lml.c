/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_lml
 *   - tester for LML render
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lml.h"
#include "log.h"
#include "trie_dict.h"
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum _test_lml_constant_t
{
	STR_OUT_BUF_SIZE = 4096,
};

static const char TRIE_DICT_SHM_FILE[] = "~trie_dict_shm.dat";

const char *str_in[] = {
	"[left]ABCD[right]EFG",
	"A[u]B[italic]CD[/i]E[/u]F[b]G[/bold]",
	"A[url BC DE]测试a网址[/url]FG",
	"AB[email CDE]F[/eMAil]G01[emaiL]23456[/email]789",
	"A[user DE]BC[/User]FG",
	"[article A B CD]EF[  /article]G[article 789]123[/article]456",
	"A[ image  BCD]EFG",
	"AB[ Flash  CDE ]FG",
	"AB[bwf]CDEFG",
	"[lef]A[rightBCD[right]EF[left[left[]G[left",
	"A[ color  BCD]EF[/color]G[color black]0[/color][color magenta]1[color cyan]23[/color]4[color red]5[/color]6[color yellOw]7[/color]8[color green]9[color blue]0[/color]",
	"A[quote]B[quote]C[quote]D[quote]E[/quote]F[/quote]G[/quote]0[/quote]1[/quote]2[quote]3[/quote]4[/quote]56789",
	": ABCDE[quote]FG\r\nab[/quote]cd[quote]ef[quote]g\r\n: : 012[/quote]345[/quote]6789\nABC[quote]DEFG",
	"\033[1;35;42mABC\033[0mDE\033[334mF\033[33mG\033[12345\033[m",
	"123456",
	"[color red]Red[/color][plain][color blue]Blue[/color][plain]",
	"[color yellow]Yellow[/color][nolml][left][color blue]Blue[/color][right][lml][color red]Red[/color]",
	"[abc][left ][ right ][ colory ][left  \nABCD[left]EFG[right ",
	"ABCD]EFG",
	": : A123456789B123456789C123456789D123456789E123456789F123456789G123456789H123456789I123456789J123456789",
	"\033[0m\033[I             \033[1;32m;,                                           ;,\033[m",
	"\n01234567890123456789012345678901234567890123456789012345678901234567890123456789\n2\n01234567890123456789012345678901234567890123456789012345678901234567890123456789\n4\n5\n",
	"A[012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789]B",
	"[nolml]发信人：feilan.bbs@bbs.sjtu.edu.cn (蓝，本是路人)，信区：cn.bbs.sci.medicine\n"
	"标  题：Re: 阑尾有益寿延年功能 尾骨是有用的“小尾巴”\n"
	"发信站：饮水思源\n"
	"转信站：LeafOK!news.ccie.net.cn!SJTU\n"
	"\n"
	"昏\n"
	"当然是阑尾\n"
	"【 在 lang (浪子~继续减肥&戒酒) 的大作中提到: 】\n"
	": 阑尾?\n"
	": 扁桃体?\n"
	": 还是尾椎?\n"
	": 【 在 feilan (蓝，本是路人) 的大作中提到: 】\n"
	": : -________________-!!!\n"
	": : 完了\n"
	": : 我已经割掉了\n"
	": : 555555555555\n"
	": : ",
	"[image http://us.ent4.yimg.com/movies.yahoo.com/images/hv/photo/movie_pix/images/hv/photo/movie_pix/]\n",
	"[tag-name-buffer-overflow abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789]\n",
};

const int str_cnt = sizeof(str_in) / sizeof(const char *);

int main(int argc, char *argv[])
{
	clock_t clock_begin;
	clock_t clock_end;
	double prog_time_spent;
	double lml_time_spent;

	char str_out_buf[STR_OUT_BUF_SIZE];
	FILE *fp;
	int i;
	int j;

	// Apply the specified locale
	if (setlocale(LC_ALL, "en_US.UTF-8") == NULL)
	{
		fprintf(stderr, "setlocale(LC_ALL, en_US.UTF-8) error\n");
		return -1;
	}

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	if ((fp = fopen(TRIE_DICT_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error", TRIE_DICT_SHM_FILE);
		return -1;
	}
	fclose(fp);

	if (trie_dict_init(TRIE_DICT_SHM_FILE, TRIE_NODE_PER_POOL) < 0)
	{
		printf("trie_dict_init failed\n");
		return -1;
	}

	clock_begin = clock();

	if (lml_init() < 0)
	{
		printf("lml_init() error\n");
		log_end();
		return -1;
	}

	printf("Test #1: quote_mode = 0\n");
	for (i = 0; i < str_cnt; i++)
	{
		j = lml_render(str_in[i], str_out_buf, sizeof(str_out_buf), 80, 0);

		printf("Input(len=%ld): %s\nOutput(len=%d): %s\n",
			   strlen(str_in[i]), str_in[i], j, str_out_buf);
		if (j != strlen(str_out_buf))
		{
			printf("Output len(%ld) != ret(%d)\n", strlen(str_out_buf), j);
			return -1;
		}
	}
	printf("Test #1: Done\n\n");

	printf("Test #2: quote_mode = 1\n");
	for (i = 0; i < str_cnt; i++)
	{
		j = lml_render(str_in[i], str_out_buf, sizeof(str_out_buf), 80, 1);

		printf("Input(len=%ld): %s\nOutput(len=%d): %s\n",
			   strlen(str_in[i]), str_in[i], j, str_out_buf);
		if (j != strlen(str_out_buf))
		{
			printf("Output len(%ld) != ret(%d)\n", strlen(str_out_buf), j);
			return -1;
		}
	}
	printf("Test #2: Done\n\n");

	clock_end = clock();
	prog_time_spent = (double)(clock_end - clock_begin) / (CLOCKS_PER_SEC / 1000);
	lml_time_spent = (double)lml_total_exec_duration / (CLOCKS_PER_SEC / 1000);

	printf("\npage_exec_duration=%.2f, lml_exec_duration=%.2f\n", prog_time_spent, lml_time_spent);

	lml_cleanup();
	trie_dict_cleanup();

	if (unlink(TRIE_DICT_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error", TRIE_DICT_SHM_FILE);
		return -1;
	}

	log_end();

	return 0;
}
