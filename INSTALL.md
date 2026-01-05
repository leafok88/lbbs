Installation
==================
To install LBBS on Linux (e.g. Debian 13, CentOS Stream 10), please perform the following steps:

0) Prerequisite  
   Follow README.md under [leafok_bbs](https://github.com/leafok/leafok_bbs) to initialize the database structure shared by both web version and telnet version.   
   It is highly recommended to finish the configuration steps of web version first and make sure those features could work properly.

1) Common requirements  
   gcc >= 13.3  
   autoconf >= 2.68  
   automake >= 1.16  
   php >= 8.2  
   mysql >= 8.4  
   (For Debian / Ubuntu)  
   sudo apt-get install -y libssh-dev libsystemd-dev  
   (For CentOS / RHEL)  
   sudo dnf install -y libssh-devel systemd-devel  
   (For MSYS2 with MinGW-w64 toolchain)  
   pacman -S --needed msys2-runtime-devel libssh libssh-devel pcre2-devel mingw-w64-x86_64-libiconv mingw-w64-x86_64-libmariadbclient  

3) Extract the source files from a tarball or export from GitHub  
   Run the following command to set up the autoconf/automake environment,  
   autoreconf --install --force

4) Compile source files  
   export LBBS_HOME_DIR=/usr/local/lbbs  
   (For Linux)  
   ./configure --prefix=$LBBS_HOME_DIR  
   (For MSYS2 with MinGW-w64 toolchain)  
   ./configure --prefix=$LBBS_HOME_DIR --disable-shared PKG_CONFIG_PATH=/mingw64/lib/pkgconfig/  
   make  

5) Create user and group  
   sudo useradd bbs

6) Install binary files and data files  
   sudo make install  
   chown -R bbs:bbs $LBBS_HOME_DIR

7) Modify following configuration files  
   Default configuration files is saved as *.default, you should rename them first.  
   $LBBS_HOME_DIR/conf/bbsd.conf  
   $LBBS_HOME_DIR/conf/bbsnet.conf  
   $LBBS_HOME_DIR/conf/badwords.conf  
   $LBBS_HOME_DIR/utils/conf/db_conn.conf.php  

8) Copy CA cert file of MySQL server to $LBBS_HOME_DIR/conf/ca_cert.pem  

9) Generate menu configuration file with section data by running the script  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_section_menu.php  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_ex_list.php  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_top.php  

10) Create SSH2 RSA / ED25519 certificate  
   ssh-keygen -t rsa -C "Your Server Name" -f $LBBS_HOME_DIR/conf/ssh_host_rsa_key  
   ssh-keygen -t ed25519 -C "Your Server Name" -f $LBBS_HOME_DIR/conf/ssh_host_ed25519_key  
   ssh-keygen -t ecdsa -C "Your Server Name" -f $LBBS_HOME_DIR/conf/ssh_host_ecdsa_key  

11) Startup  
   sudo -u bbs $LBBS_HOME_DIR/bin/bbsd

12) (Optional) Set up systemd  
   Create your own /usr/lib/systemd/system/lbbs.service from the sample at conf/lbbs.service, and make any change if necessary.  
   Reload daemon config and start the service.  

13) (Optional) Set up logrotate  
   Create your own /etc/logrotate.d/lbbs from the sample at conf/lbbs.logrotate, and make any change if necessary.  
   Restart logrotate service.  

14) Cleanup on abnormal service termination  
   In case of any unexpected failure or improper operation which results in abnormal termination of lbbs process, manual cleanup of shared memory / semaphore might be required before re-launch the process. Run the following command to check first:  
   sudo -u bbs ipcs  
   There should be no item owned by bbs. Otherwise, run the following command to cleanup:  
   sudo -u bbs ipcrm -a


For Docker user
==================
You may either build the docker image from source code by running:  
   docker compose up --build -d  

or pull the docker image per release from Docker Hub by running:  
   docker compose pull  

You should always create / update the configuration files for local configuration (e.g. database connection, network port) as described above.  
