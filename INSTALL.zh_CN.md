安装说明
==================
请按照以下步骤进行LBBS的编译和安装：

0) 准备工作  
   按照[leafok_bbs](https://github.com/leafok88/leafok_bbs)下README.md的说明，初始化Web/Telnet版本共享的数据库。   
   强烈建议先按照Web版本的说明完成基础功能的配置和验证，再开始Telnet版本的安装。

1) 通用工具/基础库依赖  
   gcc >= 14.2.0  
   autoconf >= 2.68  
   automake >= 1.16  
   libssh >= 0.11.1  
   PHP >= 8.2  
   MySQL >= 8.4

2) 从Github导出或下载源代码文件  
   运行以下命令来初始化autoconf/automake环境：  
   sh ./autogen.sh

3) 编译源代码  
   ./configure --prefix=/usr/local/lbbs  
   make

4) 建立用户和组  
   sudo useradd bbs

5) 安装程序文件和数据文件  
   sudo make install  
   chown -R bbs:bbs /usr/local/lbbs

6) 修改以下配置文件  
   默认配置文件被命名为*.default，请先将其改名。  
   /usr/local/lbbs/conf/bbsd.conf  
   /usr/local/lbbs/utils/conf/db_conn.conf.php  

7) 运行以下脚本来生成菜单配置文件和版块精华区数据文件  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_section_menu.php  
   sudo -u bbs php /usr/local/lbbs/utils/bin/gen_ex_list.php  

8) 创建SSH2 RSA证书，并将其拷贝到/usr/local/lbbs/conf  
   ssh-keygen -t rsa -C "Your Server Name" -f ssh_host_rsa_key

9) 启动服务程序  
   sudo -u bbs /usr/local/lbbs/bin/bbsd

10) 配置systemd  
   基于conf/lbbs.service.default创建/usr/lib/systemd/system/lbbs.service，并进行必要的修改。  
   刷新配置并启动服务。

11) 服务异常终止时的清理  
   由于未预期的错误或者不当操作导致lbbs进程异常终止时，在重启服务之前可能需要进行共享内存/信号量的清理。先运行以下命令检测：  
   sudo -u bbs ipcs  
   正常情况下不存在所有者是bbs的项。否则，请运行以下命令清理：  
   sudo -u bbs ipcrm -a
