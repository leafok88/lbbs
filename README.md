# LBBS

Copyright (C) 2004-2025 by Leaflet

Email : leaflet@leafok.com


Introduction
=================
This software (named as LBBS) aims to providing a telnet-based interface for a pure web-based BBS (named as LeafOK BBS).

LeafOK BBS is a powerful BBS system, not only providing almost every function that a standard BBS should have, but also giving many features other BBS hasn't complemented yet.

As an open source project, LeafOK BBS use MySQL as its major data depository, and PHP as its developing language. With MySQL and PHP, LeafOK BBS becomes a platform-independent system. It can works stably on many popular Web Servers such as Apache and IIS.

However, as a pure web-based BBS, its shortage is inevitable. For LeafOK BBS uses Browser/Server as its architecture, it will cause more data traffic between Users and Server than telnet-based BBS, and its response is a little slower than telnet-based BBS.

In order to conquer these shortage, a new project was launched. LBBS is a telnet-based BBS which is full compatible with LeafOK BBS. It also uses MySQL as its data repository, and keep coherence with the data structure of LeafOK BBS. LBBS is designed to be a system running on Linux, with GNU C as its developing language.

Thank you for using this software. If you meet any bugs or have any suggestion, please tell me.


System Requirement
==================
Operation System: Linux

Software: 
1) GNU C Compiler
2) PHP ( Version >= 8.2 )
3) MySQL database ( Version >= 8.4 )


Quick Installation
==================
To install LBBS quickly, please do the following steps:

1) Extract the source files from a tarball or export from GitHub

   Run the following command to set up the autoconf/automake environment,

   sh ./autogen.sh

   and fix any error if exists.

2) Compile source files

   ./configure --prefix=/usr/local/lbbs

   make

3) Create user and group

   sudo useradd bbs

4) Install binary files and data files

   sudo make install

5) Modify following configuration files

   Default configuration files is saved as *.default, you should rename them first.
   
   /usr/local/lbbs/conf/bbsd.conf
   
   /usr/local/lbbs/utils/conf/db_conn.inc.php

7) Generate menu configuration file with section data by running the script

   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_section_menu.php

6) Startup

   sudo /usr/local/lbbs/bin/bbsd

7) Set up systemd

   Create your own /usr/lib/systemd/system/lbbs.service from the sample at conf/lbbs.service.default, and make any change if necessary. Please note that the startup argument "-f" with bbsd should be used in systemd notify mode.

   Run the following bash command to startup the service:

   sudo systemctl daemon-reload

   sudo systemctl start lbbs

