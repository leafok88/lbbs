/***************************************************************************
                                  LBBS
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

Introduction
============

    This software (named as LBBS) aims to providing a telnet-based interface for
a pure web-based BBS (named as LeafOK BBS).
    LeafOK BBS is a powerful BBS system, not only providing almost every function
that a standard BBS should have, but also giving many features other BBS hasn't
complemented yet.
    As an open source project, LeafOK BBS use MySQL as its major data depository,
and PHP as its developing language. With MySQL and PHP, LeafOK BBS becomes a
platform-independent system. It can works stably on many popular Web Servers
such as Apache and IIS.
    However, as a pure web-based BBS, its shortage is inevitable. For LeafOK BBS
uses Browser/Server as its architecture, it will cause more data traffic between
Users and Server than telnet-based BBS, and its response is a little slower than
telnet-based BBS.
    In order to conquer these shortage, a new project was launched. LBBS is a
telnet-based BBS which is full compatible with LeafOK BBS. It also uses MySQL as
its data repository, and keep coherence with the data structure of LeafOK BBS.
    LBBS is designed to be a system running on Linux, with GNU C as its developing
language. For my spare time is very limit, I decide not to make LBBS as a
cross-platform system temporarily. If you have interest in developing this system,
please contact me.
    Thank you for using this software. If you meet any bugs or have any suggestion,
please tell me.


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

3) Install binary files and data files
   make install

4) Create user and group
   groupadd bbs
   useradd bbs

5) Set privileges of files
   cd /usr/local/lbbs
   chown bbs:bbs -R lbbs
   chmod 750 -R lbbs
   chmod 4750 lbbs/bin/bbsd

6) Modify following configuration files
   Default configuration files is saved as *.default, you should rename them first.
   /usr/local/lbbs/conf/bbsd.conf
   /usr/local/lbbs/utils/conf/db_conn.inc.php

7) Startup
   /usr/local/lbbs/bin/bbsd

