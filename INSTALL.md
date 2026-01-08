# Installation

The Chinese version of this changelog is available at [INSTALL.zh_CN.md](INSTALL.zh_CN.md).

To install LBBS on Linux (e.g., Debian 13, CentOS Stream 10), please perform the following steps:

## 0. Prerequisites

Follow the instructions in [leafok_bbs](https://github.com/leafok/leafok_bbs) README.md to initialize the database structure shared by both the web and telnet versions.

It is highly recommended to complete the web version configuration steps first and ensure those features work properly.

## 1. Common Requirements

- gcc >= 13.3
- autoconf >= 2.68
- automake >= 1.16
- php >= 8.2
- mysql >= 8.4

### Distribution-Specific Packages

**Debian / Ubuntu:**
```bash
sudo apt-get install -y libssh-dev libsystemd-dev
```

**CentOS / RHEL:**
```bash
sudo dnf install -y libssh-devel systemd-devel
```

**MSYS2 with MinGW-w64 toolchain:**
```bash
pacman -S --needed msys2-runtime-devel libssh libssh-devel pcre2-devel mingw-w64-x86_64-libiconv mingw-w64-x86_64-libmariadbclient
```

## 2. Extract Source Files

Extract the source files from a tarball or export from GitHub.

Run the following command to set up the autoconf/automake environment:
```bash
autoreconf --install --force
```

## 3. Compile Source Files

```bash
export LBBS_HOME_DIR=/usr/local/lbbs
```

**For Linux:**
```bash
./configure --prefix=$LBBS_HOME_DIR
```

**For MSYS2 with MinGW-w64 toolchain:**
```bash
./configure --prefix=$LBBS_HOME_DIR --disable-shared PKG_CONFIG_PATH=/mingw64/lib/pkgconfig/
```

```bash
make
```

## 4. Create User and Group

```bash
sudo useradd bbs
```

## 5. Install Binary and Data Files

```bash
sudo make install
chown -R bbs:bbs $LBBS_HOME_DIR
```

## 6. Modify Configuration Files

Default configuration files are saved as `*.default`. Rename them first:

- `$LBBS_HOME_DIR/conf/bbsd.conf`
- `$LBBS_HOME_DIR/conf/bbsnet.conf`
- `$LBBS_HOME_DIR/conf/badwords.conf`
- `$LBBS_HOME_DIR/utils/conf/db_conn.conf.php`

## 7. Copy MySQL CA Certificate

Copy the MySQL server's CA certificate file to `$LBBS_HOME_DIR/conf/ca_cert.pem`.

## 8. Generate Menu Configuration Files

Run the following scripts to generate menu configuration files with section data:

```bash
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_section_menu.php
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_ex_list.php
sudo -u bbs php $LBBS_HOME_DIR/utils/bin/gen_top.php
```

## 9. Create SSH2 Certificates

```bash
ssh-keygen -t rsa -C "Your Server Name" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_rsa_key
ssh-keygen -t ed25519 -C "Your Server Name" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_ed25519_key
ssh-keygen -t ecdsa -C "Your Server Name" -N "" -f $LBBS_HOME_DIR/conf/ssh_host_ecdsa_key
```

## 10. Startup

```bash
sudo -u bbs $LBBS_HOME_DIR/bin/bbsd
```

## 11. (Optional) Set Up systemd

Create your own `/usr/lib/systemd/system/lbbs.service` from the sample at `conf/lbbs.service`, and make any necessary changes.

Reload daemon configuration and start the service.

## 12. (Optional) Set Up logrotate

Create your own `/etc/logrotate.d/lbbs` from the sample at `conf/lbbs.logrotate`, and make any necessary changes.

Restart the logrotate service.

## 13. Cleanup on Abnormal Service Termination

In case of unexpected failure or improper operation resulting in abnormal termination of the LBBS process, manual cleanup of shared memory/semaphore might be required before relaunching the process.

First, check with:
```bash
sudo -u bbs ipcs
```

There should be no items owned by `bbs`. Otherwise, clean up with:
```bash
sudo -u bbs ipcrm -a
```

# For Docker Users

You may either build the Docker image from source code by running:
```bash
docker compose up --build -d
```

or pull the Docker image per release from Docker Hub by running:
```bash
docker compose pull
```

You should always create/update the configuration files for local configuration (e.g., database connection, network port) as described above.