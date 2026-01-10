# 安装说明

英文版本的安装说明位于 [INSTALL.md](INSTALL.md)。

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

默认配置文件保存为 `*.default`。首先复制并重命名它们：

```bash
cd $LBBS_HOME_DIR
cp conf/bbsd.conf.default conf/bbsd.conf
cp conf/bbsnet.conf.default conf/bbsnet.conf
cp conf/badwords.conf.default conf/badwords.conf
cp utils/conf/db_conn.conf.php.default utils/conf/db_conn.conf.php
```

然后编辑每个文件以匹配您的环境：

### bbsd.conf
需要调整的关键设置：
- `db_host`, `db_username`, `db_password`, `db_database`: MySQL 连接详情
- `bbs_server`, `bbs_port`, `bbs_ssh_port`: 网络设置
- `bbs_name`: 您的 BBS 名称
- `bbs_max_client`: 最大并发连接数（根据服务器容量调整）

### db_conn.conf.php
设置数据库连接参数：
- `$DB_hostname`, `$DB_username`, `$DB_password`, `$DB_database`

### bbsnet.conf & badwords.conf
根据您的 BBS 策略进行审查和自定义。

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

为 SSH 服务器组件生成 SSH 主机密钥。`-N ""` 标志为密钥设置空密码（自动化服务启动所需）。

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

### 何时需要清理
- `bbsd` 进程崩溃或被强制终止后
- 如果 `bbsd` 启动时出现"共享内存已存在"错误
- 当 `ipcs` 显示属于用户 `bbs` 的资源时

### 检查孤儿资源
首先，检查是否有剩余的共享内存段或信号量：
```bash
sudo -u bbs ipcs
```

查看 "SHM"（共享内存）和 "SEM"（信号量）部分中 "OWNER" 为 `bbs` 的条目。

### 清理
如果存在资源，使用以下命令删除：
```bash
sudo -u bbs ipcrm -a
```

这将删除 `bbs` 用户可访问的所有共享内存和信号量资源。

## 14. Docker 安装（替代方法）

对于容器化部署，LBBS 提供 Docker 支持。

### 从源代码构建
要从源代码构建 Docker 镜像：
```bash
docker compose up --build -d
```

### 使用预构建镜像
要使用 Docker Hub 中的预构建镜像：
```bash
docker compose pull
docker compose up -d
```

### Docker 配置
使用 Docker 时，您仍然需要适当配置 LBBS：

1. **配置文件**：按照步骤 6 中的说明创建和自定义配置文件。
2. **数据库连接**：确保 `db_conn.conf.php` 指向您的 MySQL 服务器（容器应能访问该服务器）。
3. **端口映射**：默认情况下，Docker Compose 映射：
   - SSH 端口：2222 → 22（容器）
   - Telnet 端口：2323 → 23（容器）
   
   如果需要，请在 `docker-compose.yml` 中调整这些设置。
4. **持久化数据**：`data/` 和 `conf/` 目录作为卷挂载以实现持久化。

### Docker Compose 管理
- 启动：`docker compose up -d`
- 停止：`docker compose down`
- 查看日志：`docker compose logs -f`
- 重启：`docker compose restart`

有关更多详细信息，请参阅 `docker-compose.yml` 文件和 Docker 文档。
