/***************************************************************************
					     test_lml.c  -  description
							 -------------------
	Copyright            : (C) 2004-2025 by Leaflet
	Email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "lml.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STR_OUT_BUF_SIZE 256

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
	"\033[35mabc\033[m",
	"123456",
	"[color red]Red[/color][plain][color blue]Blue[/color][plain]",
	"[color yellow]Yellow[/color][nolml][left][color blue]Blue[/color][right][lml][color red]Red[/color]",
	"[abc][left ][ right ][ colory ][left  \nABCD[left]EFG[right ",
	"ABCD]EFG"
};

const int str_cnt = sizeof(str_in) / sizeof(const char *);

int main(int argc, char *argv[])
{
	char str_out_buf[STR_OUT_BUF_SIZE];
	int i;
	int j;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	printf("Test #1: quote_mode = 0\n");
	for (i = 0; i < str_cnt; i++)
	{
		j = lml_render(str_in[i], str_out_buf, sizeof(str_out_buf), 0);

		printf("Input(len=%ld): %s\nOutput(len=%d): %s\n", strlen(str_in[i]), str_in[i], j, str_out_buf);
	}
	printf("Test #1: Done\n");

	printf("Test #2: quote_mode = 1\n");
	for (i = 0; i < str_cnt; i++)
	{
		j = lml_render(str_in[i], str_out_buf, sizeof(str_out_buf), 1);

		printf("Input(len=%ld): %s\nOutput(len=%d): %s\n", strlen(str_in[i]), str_in[i], j, str_out_buf);
	}
	printf("Test #2: Done\n");

	log_end();

	return 0;
}
