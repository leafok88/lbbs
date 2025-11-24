安装说明
==================
请按照以下步骤在Linux (例如: Debian 13, CentOS Stream 10) 上进行LBBS的编译和安装:

0) 准备工作  
   按照[leafok_bbs](https://github.com/leafok/leafok_bbs)下README.md的说明，初始化Web/Telnet版本共享的数据库。   
   强烈建议先按照Web版本的说明完成基础功能的配置和验证，再开始Telnet版本的安装。

1) 通用工具/基础库依赖  
   gcc >= 14.2  
   autoconf >= 2.68  
   automake >= 1.16  
   php >= 8.2  
   mysql >= 8.4  
   (Debian / Ubuntu 用户)  
   sudo apt-get install -y libssh-dev libsystemd-dev  
   (For CentOS / RHEL 用户)  
   sudo dnf install -y libssh-devel systemd-devel  
   (MSYS2 with MinGW-w64 toolchain 用户)  
   pacman -S --needed msys2-runtime-devel libssh libssh-devel pcre2-devel mingw-w64-x86_64-libiconv mingw-w64-x86_64-libmariadbclient  

2) 从Github导出或下载源代码文件  
   运行以下命令来初始化autoconf/automake环境:  
   autoreconf --install --force

3) 编译源代码  
   export LBBS_HOME_DIR=/usr/local/lbbs  
   (Linux用户)  
   ./configure --prefix=$LBBS_HOME_DIR  
   (MSYS2用户: 需要MinGW-w64)  
   ./configure --prefix=$LBBS_HOME_DIR --disable-shared PKG_CONFIG_PATH=/mingw64/lib/pkgconfig/  
   make

4) 建立用户和组  
   sudo useradd bbs

5) 安装程序文件和数据文件  
   sudo make install  
   chown -R bbs:bbs $LBBS_HOME_DIR

6) 修改以下配置文件  
   默认配置文件被命名为*.default，请先将其改名。  
   $LBBS_HOME_DIR/conf/bbsd.conf  
   $LBBS_HOME_DIR/conf/badwords.conf  
   $LBBS_HOME_DIR/utils/conf/db_conn.conf.php  

7) 运行以下脚本来生成菜单配置文件和版块精华区数据文件  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_section_menu.php  
   sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_ex_list.php  

8) 将MySQL服务器的CA证书文件，复制到$LBBS_HOME_DIR/conf/ca_cert.pem  

9) 创建SSH2 RSA证书  
   ssh-keygen -t rsa -C "Your Server Name" -f $LBBS_HOME_DIR/conf/ssh_host_rsa_key

10) 启动服务程序  
   sudo -u bbs $LBBS_HOME_DIR/bin/bbsd

11) (可选) 配置systemd  
   基于conf/lbbs.service创建/usr/lib/systemd/system/lbbs.service，并进行必要的修改。  
   刷新配置并启动服务。  

12) (可选) 配置logrotate  
   基于conf/lbbs.logrotate创建/etc/logrotate.d/bbsd，并进行必要的修改。  
   重启logrotate服务。  

13) 服务异常终止时的清理  
   由于未预期的错误或者不当操作导致lbbs进程异常终止时，在重启服务之前可能需要进行共享内存/信号量的清理。先运行以下命令检测:  
   sudo -u bbs ipcs  
   正常情况下不存在所有者是bbs的项。否则，请运行以下命令清理:  
   sudo -u bbs ipcrm -a

