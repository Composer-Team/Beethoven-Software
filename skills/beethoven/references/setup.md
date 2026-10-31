# Setting up Beethoven

Two kinds of machine need different setups:

- **Development machine** (where the user writes hardware, simulates, and for
  Zynq also synthesizes): needs the full toolchain — system packages,
  Java/sbt/Scala via SDKMAN, Icarus Verilog, then `beethoven setup`.
- **Deploy-only machine** (Zynq board, AWS F2 compile VM, AWS F2 instance):
  never runs Chisel, so it skips SDKMAN entirely and uses
  `beethoven setup --no-hardware`. Jump to [Deploy-only machines](#deploy-only-machines).

Work through the steps in order and verify each one before moving on.
Commands needing `sudo` must be handed to the user (see SKILL.md rule 5).

## 0. Check what is already there

```bash
uname -s; uname -m; echo "$SHELL"
command -v beethoven && beethoven --version
command -v git cmake c++ curl zip unzip iverilog
[ -d "$HOME/.sdkman" ] && echo "sdkman present"
```

Skip any step whose result is already in place. If `beethoven` exists but is
older than the latest release, replace it (step 1).

## 1. Install the `beethoven` CLI binary

Releases: https://github.com/Composer-Team/Beethoven-Software/releases.
Each asset is a tarball containing a single `beethoven` executable at the top
level, named `beethoven-<tag>-<triple>.tar.gz`:

| `uname -s` / `uname -m` | `<triple>` |
|---|---|
| Linux / x86_64 | `x86_64-unknown-linux-musl` |
| Linux / aarch64 (incl. Zynq boards) | `aarch64-unknown-linux-musl` |
| Darwin / arm64 | `aarch64-apple-darwin` |
| Darwin / x86_64 | `x86_64-apple-darwin` |

The musl builds are static, so they run on any Linux distro.

```bash
TAG=$(curl -fsSL https://api.github.com/repos/Composer-Team/Beethoven-Software/releases/latest \
      | grep -m1 '"tag_name"' | cut -d'"' -f4)
TRIPLE=x86_64-unknown-linux-musl          # pick from the table
DL=$(mktemp -d)
curl -fsSL -o "$DL/b.tar.gz" \
  "https://github.com/Composer-Team/Beethoven-Software/releases/download/$TAG/beethoven-$TAG-$TRIPLE.tar.gz"
tar -xzf "$DL/b.tar.gz" -C "$DL"
mkdir -p "$HOME/.local/bin" && install -m 755 "$DL/beethoven" "$HOME/.local/bin/beethoven"
rm -rf "$DL"
```

If `~/.local/bin` is not on `PATH`, append
`export PATH="$HOME/.local/bin:$PATH"` to the user's shell rc (`~/.bashrc` or
`~/.zshrc`, according to `$SHELL`), and prefix it in the current commands.
Verify: `beethoven --version` prints the tag.

## 2. System packages (user runs these with sudo)

Required: `git`, `cmake` ≥ 3.16, a C++17 compiler, `curl`, `zip`, `unzip`
(the last three for SDKMAN), and `iverilog` (default simulator). `verilator`
is only needed for projects that set `simulator = "verilator"`.

Ubuntu / Debian:
```bash
sudo apt-get update
sudo apt-get install -y curl build-essential zip unzip git cmake iverilog verilator rsync openssh-client
```

RHEL / Rocky / Fedora (iverilog and verilator come from EPEL on RHEL-family):
```bash
sudo dnf install -y epel-release
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y curl zip unzip git cmake iverilog verilator rsync openssh-clients
```

macOS (Homebrew; no sudo). **Install iverilog now, and sbt in step 3,
before running `beethoven setup`**; if either is missing, setup stops on a
brew Y/n prompt that an agent cannot answer:
```bash
xcode-select --install          # compiler toolchain, if missing
brew install icarus-verilog verilator cmake git rsync
```

## 3. Java, sbt, Scala via SDKMAN

Pinned versions: **java `17.0.9-graalce`, sbt `1.12.4`, scala `3.8.4`.**

```bash
curl -s "https://get.sdkman.io" | bash
```

The installer appends an init snippet to `~/.bashrc` and `~/.zshrc`. Verify
the user's actual rc (per `$SHELL`) contains `sdkman-init.sh`; add it if not:

```bash
export SDKMAN_DIR="$HOME/.sdkman"
[[ -s "$HOME/.sdkman/bin/sdkman-init.sh" ]] && source "$HOME/.sdkman/bin/sdkman-init.sh"
```

**Agent caveat:** each agent shell command starts a fresh non-interactive
shell that does not read the rc file. Every command that needs `sdk`, `java`,
or `sbt` (including `beethoven setup`, `build`, `check`, `sim`, `synth`) must
first source the init script in the same command line. Set
`sdkman_auto_answer=true` so `sdk` does not prompt:

```bash
source "$HOME/.sdkman/bin/sdkman-init.sh" && export sdkman_auto_answer=true && \
  sdk install java 17.0.9-graalce && sdk default java 17.0.9-graalce && \
  sdk install sbt 1.12.4 && sdk default sbt 1.12.4 && \
  sdk install scala 3.8.4 && sdk default scala 3.8.4
```

Verify: `source ~/.sdkman/bin/sdkman-init.sh && java -version && sbt --version && scala -version`.
(`sbt --version` is slow on first run while it bootstraps.)

## 4. `beethoven setup`

Run from any directory:

```bash
source "$HOME/.sdkman/bin/sdkman-init.sh" && beethoven setup
```

What it does: clones Beethoven-Software into a temp dir, builds and installs
`libbeethoven` into `~/.local` (writing a cmake package-registry entry so
`find_package(beethoven)` works everywhere), then clones Beethoven-Hardware to
`~/.local/share/beethoven/hardware-src` and runs `sbt publishLocal`. It takes
several minutes and needs network access. Useful flags:

| Flag | When |
|---|---|
| `--no-hardware` | Deploy-only machines (no sbt needed). |
| `--prefix <dir>` | Install somewhere other than `~/.local`. |
| `--from <path>` / `--hardware-from <path>` | Use existing local clones (developers of Beethoven itself). |
| `--ref` / `--hardware-ref` | Pin a branch/tag instead of the remote default branch. |
| `-j N` | Parallel build jobs. |

## 5. Verify end to end

```bash
beethoven info                       # prefix, runtime-src, libbeethoven path
SMOKE=$(mktemp -d) && cd "$SMOKE"
source "$HOME/.sdkman/bin/sdkman-init.sh" && \
  beethoven new smoke && cd smoke && beethoven build && timeout 900 beethoven sim
```

The generated testbench prints a `[PASS] ...` line. The first build is slow
(sbt downloads dependencies). Remove the smoke dir afterwards. Report the
versions installed and the smoke result to the user.

## Deploy-only machines

Zynq board (aarch64 Linux, e.g. Ubuntu or PetaLinux), AWS F2 compile VM, AWS
F2 instance:

1. Install the CLI binary (step 1; aarch64 triple on Zynq).
2. Packages: `git`, `cmake` ≥ 3.16, C++17 compiler, `curl`, `rsync`; add
   `tmux python3` on the F2 compile VM. Ubuntu:
   `sudo apt-get update && sudo apt-get install -y cmake build-essential git curl rsync tmux python3`.
3. `beethoven setup --no-hardware`.
4. Verify with `beethoven info`.

## Maintenance

- `beethoven update` — re-clone and reinstall libbeethoven (same `--from`,
  `--ref`, `-j` flags as setup).
- `beethoven uninstall --yes` — remove libbeethoven, the cmake registry entry,
  the CLI cache, and the user config (`~/.config/beethoven/`). Always pass
  `--yes`. Confirm with the user first.
- Upgrading the CLI binary = repeat step 1.
