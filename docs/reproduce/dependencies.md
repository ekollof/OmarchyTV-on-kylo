# Dependencies and upstream sources

Last verified: 2026-11-10 (versions observed on the original host; install
commands are upstream package names, not re-run on a clean distribution).

Everything here is an upstream tool or a host package. Vendor firmware,
libraries, keys and images are **not** dependencies you download here — extract
them from a TV you own. The tested versions are from the original development
host; newer versions usually work, but record what you use.

Run the checker after installing:

```sh
tools/check-host.sh --root   # only the NAND -> /conf root path
tools/check-host.sh          # full build/analysis toolchain
```

`--root` checks the tools needed to dump the NAND, build the `/conf` image and
restore it. The full check additionally covers Ghidra, the ARM compiler, qemu,
Node and the SQLite headers. A Ghidra/ARM/qemu miss does **not** block the root
path. The checker never installs anything and never reads the TV.

## Tested host

| tool | tested | purpose |
|---|---|---|
| Linux host | Arch rolling (2026-11) | build/run host tests, extraction, Ghidra |
| Git / make / GCC / binutils | host packages | source, builds, `objdump`/`readelf`/`nm`/`strings` |
| Python | 3.14.9 | extraction/analysis helpers, venv |
| Node.js | 26.13.0 | `.cjs` IPC/quiesce tests (built-ins only) |
| Java | OpenJDK 27.0.1 | Ghidra runtime; use a JDK supported by your Ghidra |
| Ghidra | 12.1.2 DEV (`/opt/ghidra`) | headless analysis and the `tools/re` scripts |
| ARM GNU toolchain | 10.2-2020.11 (arm-10.16), GCC 10.2.1 | ARM hard-float TV builds |
| qemu | qemu-arm 11.1.3 (`qemu-user-static`; binary may be `qemu-arm-static` or `qemu-arm`) | ARM tests with the extracted rootfs |
| mtd-utils | 2.3.1 | `mkfs.ubifs`, UBI tools |
| squashfs-tools | 4.7.5 | `unsquashfs` for root/vendor/vendor2 |
| SQLite | 3.54.1 CLI + headers | settings DB checks and `-lsqlite3` host builds |
| GDB | 18.1 | ARM/qemu crash diagnosis (host gdb) |
| ccache | 4.14.1 (optional) | faster rebuilds |
| OpenSSH | host | `ssh`, `ssh-keygen` for the TV hook |

## Install the host packages

Debian/Ubuntu (names vary by release):

```sh
sudo apt update
sudo apt install -y build-essential git openjdk-21-jdk python3 python3-venv \
  nodejs npm qemu-user-static mtd-utils squashfs-tools sqlite3 libsqlite3-dev \
  openssh-client gdb-multiarch ccache xz-utils
```

Arch:

```sh
sudo pacman -S --needed base-devel git jdk-openjdk python nodejs npm \
  qemu-user-static mtd-utils squashfs-tools sqlite openssh gdb ccache xz
```

Fedora:

```sh
sudo dnf install -y @development-tools git java-21-openjdk python3 nodejs npm \
  qemu-user-static mtd-utils squashfs-tools sqlite sqlite-devel \
  openssh-clients gdb ccache xz
```

If your distro names `qemu-user-static` differently, install `qemu-user` and
check which `qemu-arm*` binary it provides. Install a JDK that your Ghidra
release supports; the original host used OpenJDK 27, and distro packages above
may provide 21 or 27. Use the version your Ghidra release documents.

## ARM toolchain

Download the **GNU-A 10.2-2020.11** `arm-none-linux-gnueabihf` toolchain from:

<https://developer.arm.com/downloads/-/gnu-a>

```sh
tar -xf gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf.tar.xz
export ARM_CC=$PWD/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc
```

The TV is glibc 2.23/hard-float. The project avoids newer libc symbols; check
your own binaries with `readelf --version-info` / `objdump -T` and prefer
static helpers for small tools. A toolchain newer than the target is not proof
of runtime compatibility.

## Ghidra

Download from <https://ghidra.re/download/> (or the
[NSA/Ghidra releases](https://github.com/NationalSecurityAgency/ghidra/releases)).
The original analysis used **12.1.2 DEV** at `/opt/ghidra`.

```sh
export GHIDRA_HOME=/opt/ghidra
$GHIDRA_HOME/support/analyzeHeadless <project-dir> <project> \
  -process <program> -readOnly -noanalysis \
  -scriptPath openaurora/tools/re -postScript DumpFirmware.java <symbol>
```

The analysed Ghidra projects are **not shipped**: project databases are
version-specific and contain the vendor binaries. Import your own extracted
`aurora.elf`/libraries/modules and run the scripts in
[../reverse-engineering/tooling.md](../reverse-engineering/tooling.md).

## UBI extraction and kernel tooling

```sh
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install ubi_reader
.venv/bin/pip install vmlinux-to-elf   # optional, kernel reconstruction
export UBIREADER=$PWD/.venv/bin/ubireader_extract_images
export UBIREADER_PYTHON=$PWD/.venv/bin/python3
```

Keep the venv inside your checkout; `ubi_reader` console scripts embed the
creating Python's absolute path. A copied or moved venv can therefore fail with
a missing interpreter — recreate it instead of repairing the shebang.

- ubi_reader: <https://github.com/jrspruitt/ubi_reader> (also on PyPI as
  `ubi_reader`); provides `ubireader_extract_images` (whole UBI partition) and
  `ubireader_extract_files` (files, including a bare UBIFS payload image).
- vmlinux-to-elf: <https://github.com/marin-m/vmlinux-to-elf> (optional; kernel
  reconstruction).
- mtd-utils: <http://www.linux-mtd.infradead.org/> (`mkfs.ubifs`, `ubinfo`,
  `ubiattach`, `ubiformat`).
- squashfs-tools: <https://github.com/plougher/squashfs-tools> (`unsquashfs`).

## qemu and the extracted rootfs

```sh
qemu-arm-static -L <extracted-rootfs>/root openaurora/build-tv-v3/tests/<binary>
```

Install `qemu-user-static`; the exact binary may be `qemu-arm-static` or
`qemu-arm`. `<extracted-rootfs>` is your own `extract_from_dump.sh` output
(the original host used `artifacts/workbench/rootfs`). qemu does not provide
the TV's devices; it proves ARM execution with the guest rootfs, not hardware
behavior.

## TV-side optional tools

- BusyBox `telnetd` is already on stock TV; it is the minimal root shell.
- Dropbear (optional SSH): <https://github.com/mkj/dropbear>. Build for ARM (the
  TV is glibc 2.23); static is the safest option if your toolchain emits newer
  symbols. Generate host keys with `dropbearkey` (host or TV). Test the binary
  before relying on it; otherwise use telnetd.
- SQLite (optional): <https://sqlite.org/>. The hook's keepalive can use
  `/conf/sqlite3` or `/buffer/sqlite3`; if neither exists, the privacy/DB
  maintenance is skipped, but root access is unaffected.

## Not dependencies: vendor material

The NAND dump, `/conf` settings, vendor libraries, firmware, Ghidra projects
and payload images come from your own TV. Do not download or redistribute
them. The hashes in this repository identify the original set's firmware
**0.51.52.3**; a different build may have different binaries and addresses.
