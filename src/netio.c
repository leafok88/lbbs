/***************************************************************************
						  netio.c  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "common.h"
#include "io.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

char s_getc(int socket)
{
	char c;
	if (recv(socket, &c, 1, 0) > 0)
		return c;
	else
		return '\0';
}

int s_putc(int socket, char c)
{
	int ret;

	ret = send(socket, &c, 1, 0);

	return ret;
}

int s_receive(int socket, char *buffer, int buffer_length, char end_str[])
{
	char buf_str[2048];
	int buf_read, total_read;
	int buf_len = 2047;

	if (buf_len + 1 > buffer_length)
		buf_len = buffer_length - 1;

	total_read = 0;
	strcpy(buffer, "");
	while ((buf_read = recv(socket, buf_str, buf_len, 0)) > 0)
	{
		buf_str[buf_read] = '\0';
		total_read += buf_read;
		strcat(buffer, buf_str);

		buf_len = buffer_length - total_read - 1;
		if (buf_len + 1 > buffer_length)
			buf_len = buffer_length - 1;

		if (strcmp((buffer + total_read - strlen(end_str)), end_str) == 0)
			break;
		// different line-end symbol in different OS
		if (strcmp(end_str, "\r") == 0)
		{
			if (strcmp((buffer + total_read - 2), "\r\n") == 0)
				break;
		}
	}

	return total_read;
}

int s_send(int socket, const char *in_str)
{
	int ret;

	if (in_str != NULL && strlen(in_str) > 0)
	{
		ret = send(socket, in_str, strlen(in_str), 0);
	}

	return ret;
}

// from Maple-hightman
// added by flyriver, 2001.3.3
int telnetopt(int fd, char *buf, int max)
{
	unsigned char c, d, e;
	int pp = 0;
	unsigned char tmp[30];

	while (pp < max)
	{
		c = buf[pp++];
		if (c == 255)
		{
			d = buf[pp++];
			e = buf[pp++];
			iflush();
			if ((d == 253) && (e == 3 || e == 24))
			{
				tmp[0] = 255;
				tmp[1] = 251;
				tmp[2] = e;
				write(fd, tmp, 3);
				continue;
			}
			if ((d == 251 || d == 252) && (e == 1 || e == 3 || e == 24))
			{
				tmp[0] = 255;
				tmp[1] = 253;
				tmp[2] = e;
				write(fd, tmp, 3);
				continue;
			}
			if (d == 251 || d == 252)
			{
				tmp[0] = 255;
				tmp[1] = 254;
				tmp[2] = e;
				write(fd, tmp, 3);
				continue;
			}
			if (d == 253 || d == 254)
			{
				tmp[0] = 255;
				tmp[1] = 252;
				tmp[2] = e;
				write(fd, tmp, 3);
				continue;
			}
			if (d == 250)
			{
				while (e != 240 && pp < max)
					e = buf[pp++];
				tmp[0] = 255;
				tmp[1] = 250;
				tmp[2] = 24;
				tmp[3] = 0;
				tmp[4] = 65;
				tmp[5] = 78;
				tmp[6] = 83;
				tmp[7] = 73;
				tmp[8] = 255;
				tmp[9] = 240;
				write(fd, tmp, 10);
			}
		}
		else
			outc(c);
	}
	iflush();
	return 0;
}
