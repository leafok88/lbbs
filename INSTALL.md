# Installation

The Chinese version of this installation guide is available at [INSTALL.zh_CN.md](INSTALL.zh_CN.md).

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

Default configuration files are saved as `*.default`. Copy and rename them first:

```bash
cd $LBBS_HOME_DIR
cp conf/bbsd.conf.default conf/bbsd.conf
cp conf/bbsnet.conf.default conf/bbsnet.conf
cp conf/badwords.conf.default conf/badwords.conf
cp utils/conf/db_conn.conf.php.default utils/conf/db_conn.conf.php
```

Then edit each file to match your environment:

### bbsd.conf
Key settings to adjust:
- `db_host`, `db_username`, `db_password`, `db_database`: MySQL connection details
- `bbs_server`, `bbs_port`, `bbs_ssh_port`: Network settings
- `bbs_name`: Your BBS name
- `bbs_max_client`: Maximum concurrent connections (adjust based on server capacity)

### db_conn.conf.php
Set the database connection parameters:
- `$DB_hostname`, `$DB_username`, `$DB_password`, `$DB_database`

### bbsnet.conf & badwords.conf
Review and customize as needed for your BBS policies.

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

Generate SSH host keys for the SSH server component. The `-N ""` flag sets an empty passphrase for the keys (required for automated service startup).

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

### When Cleanup is Needed
- After a crash or force-kill of the `bbsd` process
- If `bbsd` fails to start with "shared memory already exists" errors
- When `ipcs` shows resources owned by user `bbs`

### Checking for Orphaned Resources
First, check for any remaining shared memory segments or semaphores:
```bash
sudo -u bbs ipcs
```

Look for entries in the "SHM" (shared memory) and "SEM" (semaphore) sections where the "OWNER" is `bbs`.

### Cleaning Up
If resources exist, remove them with:
```bash
sudo -u bbs ipcrm -a
```

This removes all shared memory and semaphore resources accessible to the `bbs` user.

## 14. Docker Installation (Alternative Method)

For containerized deployment, LBBS provides Docker support.

### Building from Source
To build the Docker image from source code:
```bash
docker compose up --build -d
```

### Using Pre-built Images
To use pre-built images from Docker Hub:
```bash
docker compose pull
docker compose up -d
```

### Configuration for Docker
When using Docker, you still need to configure LBBS appropriately:

1. **Configuration Files**: Create and customize the configuration files as described in Step 6.
2. **Database Connection**: Ensure `db_conn.conf.php` points to your MySQL server (which should be accessible from the container).
3. **Port Mapping**: By default, Docker Compose maps:
   - SSH port: 2322 → 2322 (container)
   - Telnet port: 2323 → 2323 (container)
   
   Adjust these in `docker-compose.yml` if needed.
4. **Persistent Data**: The `data/` and `conf/` directories are mounted as volumes for persistence.

### Docker Compose Management
- Start: `docker compose up -d`
- Stop: `docker compose down`
- View logs: `docker compose logs -f`
- Restart: `docker compose restart`

For more details, refer to the `docker-compose.yml` file and Docker documentation.
