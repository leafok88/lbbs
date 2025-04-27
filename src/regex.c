/***************************************************************************
                          regex.c  -  description
                             -------------------
    begin                : Mon Oct 11 2004
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

#include <stdlib.h>
#include <regex.h>

int
ireg (const char *pattern, const char *string, size_t nmatch,
      regmatch_t pmatch[])
{
  regex_t *preg;
  int cflags = 0, eflags = 0, ret;

  preg = (regex_t *) malloc (sizeof (regex_t));

  cflags = REG_EXTENDED;

  if (pmatch == NULL)
    cflags |= REG_NOSUB;

  if (regcomp (preg, pattern, cflags) != 0)
    {
      log_error ("Compile regular expression pattern failed\n");
      free (preg);
      return -1;
    }
  ret = regexec (preg, string, nmatch, pmatch, eflags);

  regfree (preg);
  free (preg);

  //For debug
  //log_std(ret?"error\n":"ok\n");

  return ret;
}
