Installation
==================
To install LBBS, please perform the following steps:

0) Prerequisite  
   Follow README.md under [leafok_bbs](https://github.com/leafok88/leafok_bbs) to initialize the database structure shared by both web version and telnet version.   
   It is highly recommended to finish the configuration steps of web version first and make sure those features could work properly.

1) Common requirements  
   gcc >= 14.2.0  
   autoconf >= 2.68  
   automake >= 1.16  
   libssh >= 0.11.1  
   PHP >= 8.2  
   MySQL >= 8.4

2) Extract the source files from a tarball or export from GitHub  
   Run the following command to set up the autoconf/automake environment,  
   sh ./autogen.sh

3) Compile source files  
   ./configure --prefix=/usr/local/lbbs  
   make

4) Create user and group  
   sudo useradd bbs

5) Install binary files and data files  
   sudo make install

6) Modify following configuration files  
   Default configuration files is saved as *.default, you should rename them first.  
   /usr/local/lbbs/conf/bbsd.conf  
   /usr/local/lbbs/utils/conf/db_conn.conf.php  

7) Generate menu configuration file with section data by running the script  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_section_menu.php  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_ex_list.php  

8) Create or copy SSH2 RSA certificate into /usr/local/lbbs/conf  
   ssh-keygen -t rsa -C "Your Server Name" -f ssh_host_rsa_key

9) Startup  
   sudo /usr/local/lbbs/bin/bbsd

10) Set up systemd  
   Create your own /usr/lib/systemd/system/lbbs.service from the sample at conf/lbbs.service.default, and make any change if necessary.  
   Reload daemon config and start the service.  

