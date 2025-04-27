/***************************************************************************
                          main.c  -  description
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

#include "bbs.h"
#include "common.h"
#include "io.h"
#include "menu.h"
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void
app_help (void)
{
  prints ("Usage: bbsd [-fhv] [...]\n\n"
	  "-f\t--foreground\t\tForce program run in foreground\n"
	  "-h\t--help\t\t\tDisplay this help message\n"
	  "-v\t--version\t\tDisplay version information\n"
	  "\t--display-log\t\tDisplay standard log information\n"
	  "\t--display-error-log\tDisplay error log information\n"
	  "\n    If meet any bug, please report to <leaflet@leafok.com>\n\n");
}

void
arg_error (void)
{
  prints ("Invalid arguments\n");
  app_help ();
}

int
main (int argc, char *argv[])
{
  char log_dir[256], file_log_std[256], file_log_error[256], file_config[256];
  int i, j;
  int daemon = 1, std_log_redir = 0, error_log_redir = 0;

  //Parse args
  for (i = 1; i < argc; i++)
    {
      switch (argv[i][0])
	{
	case '-':
	  if (argv[i][1] != '-')
	    {
	      for (j = 1; j < strlen (argv[i]); j++)
		{
		  switch (argv[i][j])
		    {
		    case 'f':
		      daemon = 0;
		      break;
		    case 'h':
		      app_help ();
		      exit (0);
		    case 'v':
		      puts (app_version);
		      exit (0);
		    default:
		      arg_error ();
		      exit (1);
		    }
		}
	    }
	  else
	    {
	      if (strcmp (argv[i] + 2, "foreground") == 0)
		{
		  daemon = 0;
		  break;
		}
	      if (strcmp (argv[i] + 2, "help") == 0)
		{
		  app_help ();
		  exit (0);
		}
	      if (strcmp (argv[i] + 2, "version") == 0)
		{
		  puts (app_version);
		  exit (0);
		}
	      if (strcmp (argv[i] + 2, "display-log") == 0)
		{
		  std_log_redir = 1;
		}
	      if (strcmp (argv[i] + 2, "display-error-log") == 0)
		{
		  error_log_redir = 1;
		}
	    }
	  break;
	}
    }

  //Initialize daemon
  if (daemon)
    init_daemon ();

  //Change current dir
  strncpy (app_home_dir, argv[0], rindex (argv[0], '/') - argv[0] + 1);
  strcat (app_home_dir, "../");
  chdir (app_home_dir);

  //Initialize log
  strcpy (app_temp_dir, "/tmp/lbbs/");
  mkdir (app_temp_dir, 0777);
  strcpy (log_dir, app_home_dir);
  strcat (log_dir, "log/");
  strcpy (file_log_std, log_dir);
  strcpy (file_log_error, log_dir);
  strcat (file_log_std, "bbsd.log");
  strcat (file_log_error, "error.log");
  mkdir (log_dir, 0750);
  if (log_begin (file_log_std, file_log_error) < 0)
    exit (-1);

  if ((!daemon) && std_log_redir)
    log_std_redirect (2);
  if ((!daemon) && error_log_redir)
    log_err_redirect (3);

  //Load configuration
  strcpy (file_config, app_home_dir);
  strcat (file_config, "conf/bbsd.conf");
  if (load_conf (file_config) < 0)
    exit (-2);

  //Load menus
  strcpy (file_config, app_home_dir);
  strcat (file_config, "conf/menu.conf");
  if (load_menu (&bbs_menu, file_config) < 0)
    exit (-3);

  //Set signal handler
  signal (SIGCHLD, child_exit);
  signal (SIGTERM, system_exit);
  signal (SIG_RELOAD_MENU, reload_bbs_menu);
  
  //Initialize socket server
  net_server (BBS_address, BBS_port);

  //Wait for child process exit
  while (SYS_child_process_count > 0)
  {
    log_std (".");
    sleep(1);
  }
  
  //Cleanup
  unload_menu (&bbs_menu);
  rmdir (app_temp_dir);

  return 0;
}
