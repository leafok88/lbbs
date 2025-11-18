Installation
==================
To install LBBS on Linux (e.g. Debian 13, CentOS Stream 10), please perform the following steps:

0) Prerequisite  
   Follow README.md under [leafok_bbs](https://github.com/leafok/leafok_bbs) to initialize the database structure shared by both web version and telnet version.   
   It is highly recommended to finish the configuration steps of web version first and make sure those features could work properly.

1) Common requirements  
   gcc >= 14.2  
   autoconf >= 2.68  
   automake >= 1.16  
   libssh >= 0.11  
   pcre2 >= 10.38  
   php >= 8.2  
   mysql >= 8.4  

3) Extract the source files from a tarball or export from GitHub  
   Run the following command to set up the autoconf/automake environment,  
   autoreconf --install --force

4) Compile source files  
   export LBBS_HOME_DIR=/usr/local/lbbs  
   ./configure --prefix=$LBBS_HOME_DIR  
   make

5) Create user and group  
   sudo useradd bbs

6) Install binary files and data files  
   sudo make install  
   chown -R bbs:bbs $LBBS_HOME_DIR

7) Modify following configuration files  
   Default configuration files is saved as *.default, you should rename them first.  
   $LBBS_HOME_DIR/conf/bbsd.conf  
   $LBBS_HOME_DIR/conf/badwords.conf  
   $LBBS_HOME_DIR/utils/conf/db_conn.conf.php  

8) Generate menu configuration file with section data by running the script  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_section_menu.php  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_ex_list.php  

9) Create SSH2 RSA certificate  
   ssh-keygen -t rsa -C "Your Server Name" -f $LBBS_HOME_DIR/conf/ssh_host_rsa_key

10) Startup  
   sudo -u bbs $LBBS_HOME_DIR/bin/bbsd

11) (Optional) Set up systemd  
   Enable systemd related feature during config, by running:  
     ./configure --prefix=$LBBS_HOME_DIR --enable-systemd
   Create your own /usr/lib/systemd/system/lbbs.service from the sample at conf/lbbs.service, and make any change if necessary.  
   Reload daemon config and start the service.  

12) (Optional) Set up logrotate  
   Create your own /etc/logrotate.d/lbbs from the sample at conf/lbbs.logrotate, and make any change if necessary.  
   Restart logrotate service.  

13) Cleanup on abnormal service termination  
   In case of any unexpected failure or improper operation which results in abnormal termination of lbbs process, manual cleanup of shared memory / semaphore might be required before re-launch the process. Run the following command to check first:  
   sudo -u bbs ipcs  
   There should be no item owned by bbs. Otherwise, run the following command to cleanup:  
   sudo -u bbs ipcrm -a


For Cygwin User
==================
Installation of LBBS on Cygwin is similar to the above steps for Linux, except that:

a. Use additional config switches to keep compatible with the Cygwin environment.  
   ./configure --prefix=$LBBS_HOME_DIR --with-mariadb --without-epoll  

b. Enable Cygserver with optimized parameters (as large as possible) of XSI semaphore and shared memory in /etc/cygserver.conf  

c. Skip those steps not supported by Cygwin (e.g. systemd and logrotate).  
