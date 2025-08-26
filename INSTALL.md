Installation
==================
To install LBBS, please perform the following steps:

0) Prerequisite  
   Follow README.md under [leafok_bbs](https://github.com/leafok88/leafok_bbs) to initialize the database structure shared by both web version and telnet version.   
   It is highly recommended to finish the configuration steps of web version first and make sure those features could work properly.

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

6) Generate menu configuration file with section data by running the script  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_section_menu.php  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_ex_list.php  

7) Create or copy SSH2 RSA certificate into /usr/local/lbbs/conf  
   ssh-keygen -t rsa -C "Your Server Name" -f ssh_host_rsa_key

8) Startup  
   sudo /usr/local/lbbs/bin/bbsd

9) Set up systemd  
   Create your own /usr/lib/systemd/system/lbbs.service from the sample at conf/lbbs.service.default, and make any change if necessary.  
   Reload daemon config and start the service.  
