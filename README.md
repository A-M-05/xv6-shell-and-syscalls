# xv6 Shell & Kernel Extensions

A custom Unix-style shell and kernel-level extensions built for [xv6-riscv](https://github.com/mit-pdos/xv6-riscv), a minimal RISC-V teaching operating system modeled after Unix v6.

> **Note:** This repo contains only the files I authored or modified. To run this project, overlay these files onto a clean clone of [xv6-riscv](https://github.com/mit-pdos/xv6-riscv).

---

## Changes

### 1. `myshell` — A Custom Unix Shell (`user/myshell.c`)

A fully functional shell for xv6, built from scratch. It replaces the default xv6 shell with a richer, more capable interface.

**Features:**

- **Prompt & REPL loop** — Displays `abe$` and reads commands in a loop; exits cleanly on `Ctrl+D` with a `bye` message
- **Command parsing** — Tokenizes input according to a formal EBNF grammar, trimming leading/trailing whitespace
- **Fallback path resolution** — If a bare command name (e.g. `ls`) isn't found in the current directory, the shell automatically searches `/`, mimicking how `$PATH` works in real shells
- **Built-in commands:**
  - `cd [path]` — Changes the shell's working directory (defaults to `/`); uses `chdir()` so child processes inherit it
  - `exit [status]` — Prints `bye` and exits with the given status code (validated in range −128 to 127)
  - `about` — Prints a short description of the shell and its author
- **I/O redirection** — Supports `<` (stdin from file), `>` (stdout to file), and `!` (stderr to file); output files are created if they don't exist and unlinked on error
- **Multi-stage pipelines** — Supports arbitrarily chained pipes (`cmd1 | cmd2 | cmd3`), correctly wiring `fork()`, `pipe()`, and `dup()` across all stages; kills dangling processes on error
- **Script mode** — When invoked as `myshell script.sh`, reads and executes commands line by line without printing a prompt
- **Comments** — Lines beginning with `#` are ignored (used in script mode)

**Example session:**

``` bash
abe$ echo hello | grep h | wc
      1       1       6
abe$ ls | grep m | wc > count
abe$ cat count
4 16 104
abe$ about > aboutfile
abe$ exit 0
bye
```

---

### 2. `ps` System Call — Kernel Process Counting

A new system call added end-to-end through the xv6 kernel stack, exposing live process state counts to user space.

**Prototype:**

```c
int ps(int *counters);
```

Fills a 4-element array with the current count of `RUNNING`, `RUNNABLE`, `SLEEPING`, and `ZOMBIE` processes. Returns `0` on success, `-1` on failure.

**Files modified/created:**

| File | What changed |
| --- | --- |
| `kernel/proc.c` | Added `kernel_ps()` — walks the proc table with proper locking and uses `copyout()` to write counts to user space |
| `kernel/sysproc.c` | Added `sys_ps()` — the kernel-side syscall wrapper that fetches the user pointer via `argaddr()` |
| `kernel/syscall.h` | Assigned syscall number `SYS_ps = 22` |
| `kernel/syscall.c` | Declared `sys_ps` and added it to the syscall dispatch table |
| `user/user.h` | Added `int ps(int *counters)` declaration for user programs |
| `user/usys.pl` | Added `entry("ps")` to generate the RISC-V `ecall` stub |
| `user/ps.c` | User-space `ps` program that calls the syscall and prints results |

**Example output:**

``` bash
$ ps
RUNNING  : 1
RUNNABLE : 0
SLEEPING : 2
ZOMBIE   : 0
```

The key implementation detail is using `copyout()` to safely transfer kernel data to a user virtual address — the same pattern used by `wait()` and `read()` in xv6.

---

### 3. Shebang Support in `exec` (`kernel/exec.c`)

Extended `exec()` so it can run text scripts, not just ELF binaries — the same mechanism that makes `#!/bin/bash` work on Linux.

**How it works:**

When `exec()` is called with a path that isn't a valid ELF binary, it now checks whether the file starts with `#!`. If so, it reads the interpreter path from the shebang line and re-invokes `exec()` as:

``` plaintext
exec(INTERPRETER, [INTERPRETER, original_script_path, NULL])
```

So if a script at `/path/to/myscript` starts with `#! /myshell`, executing it from the shell triggers:

```c
exec("/myshell", {"/myshell", "/path/to/myscript", NULL})
```

This causes `myshell` to run the script in script mode automatically, with no special handling needed in the shell itself.

---

## Extra Utilities

A few utility programs were also added to support testing and shell usage:

- **`user/pwd.c`** — Prints the current working directory by walking up the directory tree via `stat()` and reading `dirent` entries
- **`user/diff.c`** — Character-by-character file diff; reports the byte offset of the first difference
- **`user/tr.c`** — A combined `tr`/`sed`-style text transformer supporting delete, substitute, and translate modes with escape sequences

---

## How to Run

```bash
# Clone xv6
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv

# Overlay the project files (adjust path as needed)
cp -r /path/to/this/repo/kernel/* kernel/
cp -r /path/to/this/repo/user/*   user/
cp /path/to/this/repo/Makefile    .

# Build and run
make qemu

# Inside xv6, launch the shell
$ myshell
cs143a$ ps
cs143a$ echo hello | wc
```

---

## Skills Demonstrated

- Kernel development in C (no standard library)
- Full Unix syscall lifecycle: user stub → trap → dispatch table → kernel handler → `copyout` to user space
- Process management: `fork()`, `exec()`, `wait()`, `pipe()`, `dup()`
- File descriptor manipulation for I/O redirection
- ELF binary loading and shebang-based script execution in the kernel
- Building and navigating a real operating system (xv6 on RISC-V/QEMU)

---
