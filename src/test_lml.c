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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define STR_OUT_BUF_SIZE 1024

const char *str_in[] = {
	"[left]ABCD[right]EFG",
	"A[u]BCDE[/underline]FG",
	"A[url BCDE[/url]FG",
	"AB[email CDE]F[/eMAil]G01[emaiL]23456[/email]789",
	"A[user DE]BC[  /User  ]FG",
	"[article A B CD]EF[/article   ]G",
	"A[ image  BCD]EFG",
	"AB[ Flash  CDE ]FG",
	"AB[bwf]CDEFG",
	"[lef]A[rightBCD[right]EF[left[left[]G[left",
	"A[ color  BCD]EF[/color]G[color black]0[/color][color magenta]1[color cyan]23[/color]4[color red]5[/color]6[color yellOw]7[/color]8[color green]9[color blue]0[/color]",
};

int str_cnt = 11;

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

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	for (i = 0; i < str_cnt; i++)
	{
		j = lml_plain(str_in[i], str_out_buf, sizeof(str_out_buf));

		printf("Input(len=%ld): %s\nOutput(len=%d): %s\n", strlen(str_in[i]), str_in[i], j, str_out_buf);
	}

	log_end();

	return 0;
}
