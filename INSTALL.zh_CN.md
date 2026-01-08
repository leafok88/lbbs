# 安装说明

要在 Linux（例如：Debian 13、CentOS Stream 10）上安装 LBBS，请按照以下步骤操作：

## 0. 准备工作

按照 [leafok_bbs](https://github.com/leafok/leafok_bbs) 中的 README.md 说明，初始化 Web 和 Telnet 版本共享的数据库结构。

强烈建议先完成 Web 版本的配置步骤，并确保这些功能正常工作。

## 1. 通用要求

- gcc >= 13.3
- autoconf >= 2.68
- automake >= 1.16
- php >= 8.2
- mysql >= 8.4

### 发行版特定软件包

**Debian / Ubuntu：**
```bash
sudo apt-get install -y libssh-dev libsystemd-dev
```

**CentOS / RHEL：**
```bash
sudo dnf install -y libssh-devel systemd-devel
```

**MSYS2 with MinGW-w64 工具链：**
```bash
pacman -S --needed msys2-runtime-devel libssh libssh-devel pcre2-devel mingw-w64-x86_64-libiconv mingw-w64-x86_64-libmariadbclient
```

## 2. 提取源代码文件

从 tarball 或 GitHub 导出源代码文件。

运行以下命令来设置 autoconf/automake 环境：
```bash
autoreconf --install --force
```

## 3. 编译源代码

```bash
export LBBS_HOME_DIR=/usr/local/lbbs
```

**对于 Linux：**
```bash
./configure --prefix=$LBBS_HOME_DIR
```

**对于 MSYS2 with MinGW-w64 工具链：**
```bash
./configure --prefix=$LBBS_HOME_DIR --disable-shared PKG_CONFIG_PATH=/mingw64/lib/pkgconfig/
```

```bash
make
```

## 4. 创建用户和组

```bash
sudo useradd bbs
```

## 5. 安装二进制和数据文件

```bash
sudo make install
chown -R bbs:bbs $LBBS_HOME_DIR
```

## 6. 修改配置文件

默认配置文件保存为 `*.default`。首先重命名它们：

- `$LBBS_HOME_DIR/conf/bbsd.conf`
- `$LBBS_HOME_DIR/conf/bbsnet.conf`
- `$LBBS_HOME_DIR/conf/badwords.conf`
- `$LBBS_HOME_DIR/utils/conf/db_conn.conf.php`

## 7. 复制 MySQL CA 证书

将 MySQL 服务器的 CA 证书文件复制到 `$LBBS_HOME_DIR/conf/ca_cert.pem`。

## 8. 生成菜单配置文件

运行以下脚本以生成包含版块数据的菜单配置文件：

```bash
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_section_menu.php
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_ex_list.php
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_top.php
```

## 9. 创建 SSH2 证书

```bash
ssh-keygen -t rsa -C "您的服务器名称" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_rsa_key
ssh-keygen -t ed25519 -C "您的服务器名称" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_ed25519_key
ssh-keygen -t ecdsa -C "您的服务器名称" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_ecdsa_key
```

## 10. 启动

```bash
sudo -u bbs $LBBS_HOME_DIR/bin/bbsd
```

## 11. （可选）设置 systemd

从 `conf/lbbs.service` 中的示例创建您自己的 `/usr/lib/systemd/system/lbbs.service`，并进行任何必要的更改。

重新加载守护进程配置并启动服务。

## 12. （可选）设置 logrotate

从 `conf/lbbs.logrotate` 中的示例创建您自己的 `/etc/logrotate.d/lbbs`，并进行任何必要的更改。

重新启动 logrotate 服务。

## 13. 服务异常终止时的清理

如果发生意外故障或操作不当导致 LBBS 进程异常终止，在重新启动进程之前可能需要手动清理共享内存/信号量。

首先，使用以下命令检查：
```bash
sudo -u bbs ipcs
```

不应存在属于 `bbs` 的项目。否则，使用以下命令清理：
```bash
sudo -u bbs ipcrm -a
```

# 对于 Docker 用户

您可以通过运行以下命令从源代码构建 Docker 镜像：
```bash
docker compose up --build -d
```

或者通过运行以下命令从 Docker Hub 拉取每个版本的 Docker 镜像：
```bash
docker compose pull
```

您应始终按照上述说明创建/更新本地配置（例如，数据库连接、网络端口）的配置文件。