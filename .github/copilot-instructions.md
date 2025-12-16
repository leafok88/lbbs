# LBBS — Copilot Instructions

Short, specific pointers to help an AI coding agent be productive in this repo.

**Build & Test**
- **Install deps**: libssh, libpcre2 and a MySQL/MariaDB client dev package (e.g., libssh-dev, libpcre2-dev, libmariadb-dev or libmysqlclient-dev). CI also installs `libsystemd-dev`.
- **Build**: `autoreconf --install --force` then `./configure --enable-systemd --disable-silent-rules` and `make`.
- **Run tests**: `make check` (tests are custom binaries built under `src/`, e.g. `test_trie_dict`). CI runs `make check` and `make distcheck` (see [.github/workflows/makefile.yml](.github/workflows/makefile.yml#L1-L40)).

**Run & Debug**
- **Server binary**: `src/bbsd` (entry: [src/main.c](src/main.c#L1-L60)).
- **Run in foreground**: `src/bbsd -f` (use `--display-log`/`--display-error-log` to redirect logs to stderr).
- **Debug**: run with `gdb --args src/bbsd -f` or attach to the process; logs are in `log/`.

**High-level architecture**
- **Process**: `bbsd` (server) loads config and data files at start, initializes shared pools, listens for telnet/ssh clients and spawns handlers.
- **Networking**: accept loop and client lifecycle are in [src/net_server.c](src/net_server.c#L1-L140); user-facing shuttle/menu networking is in [src/bbs_net.c](src/bbs_net.c#L1-L60).
- **IO / Encoding**: terminal IO and non-blocking I/O are handled in [src/io.c](src/io.c#L1-L80). The project supports epoll/poll (conditional) and uses iconv for charset conversion. Default charset is `UTF-8`.
- **Auth & DB**: login and auth live in [src/login.c](src/login.c#L1-L40); database access is in [src/database.c](src/database.c#L1-L40). DB credentials are read from `conf/bbsd.conf` (example in [conf/bbsd.conf](conf/bbsd.conf#L1-L40)).

**Config & runtime files**
- **Configs**: `conf/` contains runtime config (e.g., `conf/bbsd.conf`, `conf/bbsnet.conf`). Config constants are centralized in [src/common.c](src/common.c#L1-L40) as `CONF_*`.
- **b b snet format**: each line is `ORG SITE HOST PORT USE_SSH(Y/N) CHARSET` (see how it's parsed in [src/bbs_net.c](src/bbs_net.c#L88-L118)).
- **SSH keys**: host keys live in `conf/` (see `SSH_HOST_*` constants in [src/common.c](src/common.c#L1-L40)).
- **Data & runtime**: static text files in `data/`; generated runtime files and caches in `var/`; logs in `log/`.

**Project patterns and conventions**
- **Autotools**: use `autoconf/automake` (`configure.ac`, `Makefile.am`); prefer `autoreconf` + `./configure` for local iterations.
- **Tests**: tests are small programs named `test_*` in `src/` and are exercised by `make check`. Add new tests as `test_<feature>.c` and register in `src/Makefile.am`.
- **Error/logging**: prefer `log_common()` / `log_error()` instead of printing to stdout. Most functions return negative on error.
- **Globals & constants**: global config names use prefixes like `BBS_`, `VAR_`, `CONF_` (see [src/common.c](src/common.c#L1-L80)).

**When changing networking/IO code**
- Run `make check` and manually run `src/bbsd -f` to exercise interactive code paths.
- For SSH-related changes, ensure `libssh` is present and try `test_ssh_server` in `src/`.
- For charset changes, check `io.c` and the per-connection iconv usage (see [src/bbs_net.c](src/bbs_net.c#L250-L320)).

**CI & packaging**
- CI uses Ubuntu runners, installs system packages and runs `./configure`, `make`, `make check`, `make distcheck` (see [.github/workflows/makefile.yml](.github/workflows/makefile.yml#L1-L80)).

If any of these sections look incomplete or you want more examples (e.g., walk-through for adding a test or adding a `configure` option), tell me which area to expand. I can iterate. (Drafted by AI; please confirm tone and level of detail.)