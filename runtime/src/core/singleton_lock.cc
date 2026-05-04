//
// singleton_lock.cc — implementation of the per-project daemon lock.
//
// See core/singleton_lock.h for the contract and the rationale.
//

#include "core/singleton_lock.h"

#include <cerrno>
#include <climits>     // PATH_MAX
#include <cstdint>
#include <cstdio>      // snprintf
#include <cstdlib>     // getenv, exit, strtol, realpath
#include <cstring>     // strerror
#include <fcntl.h>     // open, O_RDWR, O_CREAT
#include <iostream>    // std::cerr
#include <string>
#include <sys/file.h>  // flock, LOCK_EX, LOCK_NB
#include <sys/mman.h>  // shm_unlink
#include <unistd.h>    // getpid, geteuid, pwrite, ftruncate, close

#include <beethoven/runtime_ipc.h>  // cmd_server_file_name, data_server_file_name

// Some build environments don't expose PATH_MAX through <climits>
// (e.g. when targeting GNU/Hurd). 4096 is the value Linux uses for
// PATH_MAX and is more than enough for any real project root.
#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

namespace beethoven {

namespace {

// File descriptor of the held lock. We deliberately keep this in
// static storage so it lives until process exit — closing it early
// would release the lock and let a concurrent daemon start, which
// would defeat the entire purpose of this module. The kernel reaps it
// at exit either way.
//
// Sentinel `-1` means "lock not yet acquired"; this also drives the
// idempotency check in runtime_acquire_singleton_lock().
int g_lock_fd = -1;

// FNV-1a hash, 64-bit. We use this to derive a short, stable key from
// the canonicalized project root. Cryptographic strength is not
// needed: collisions only matter for lockfile-name uniqueness, and 64
// bits is overkill against the realistic upper bound on
// "concurrent projects on one user account."
//
// The constants below are the FNV-1a 64-bit offset basis and prime,
// taken straight from the FNV reference (http://www.isthe.com/chongo/tech/comp/fnv/).
std::uint64_t fnv1a64(const char *s) {
  std::uint64_t h = 14695981039346656037ULL;  // offset basis
  while (*s) {
    h ^= static_cast<unsigned char>(*s++);
    h *= 1099511628211ULL;                    // FNV prime
  }
  return h;
}

// Read BEETHOVEN_PROJECT_ROOT, canonicalize it, and FNV-hash the
// result. The canonicalization is critical — it normalizes
// `/home/me/proj`, `/home/me/proj/`, and `/home/me/./proj` to the
// same key, so the daemon and the CLI's future probe agree on the
// lockfile path regardless of which spelling the user typed.
//
// On any failure, prints a clear error and exits the daemon. We
// can't proceed without a stable key.
std::string project_key() {
  const char *root = std::getenv("BEETHOVEN_PROJECT_ROOT");
  if (root == nullptr || root[0] == '\0') {
    std::cerr << "BeethovenRuntime: BEETHOVEN_PROJECT_ROOT is not set. "
                 "The CLI sets this automatically; if launching the "
                 "daemon by hand, point it at your project root (the "
                 "directory containing Beethoven.toml)."
              << std::endl;
    std::exit(1);
  }
  // realpath with a NULL second argument allocates; we provide our
  // own buffer instead so we don't have to worry about freeing it on
  // every error path.
  char buf[PATH_MAX];
  if (realpath(root, buf) == nullptr) {
    std::cerr << "BeethovenRuntime: cannot canonicalize "
                 "BEETHOVEN_PROJECT_ROOT='"
              << root << "': " << std::strerror(errno) << std::endl;
    std::exit(1);
  }
  // 16 hex chars is plenty for collision resistance and short enough
  // that the resulting filename is human-readable when triaging.
  char hex[17];
  std::snprintf(hex, sizeof(hex), "%016lx",
                static_cast<unsigned long>(fnv1a64(buf)));
  return std::string(hex);
}

// Pick the directory we'll put the lockfile in, in this order:
//   1. $XDG_RUNTIME_DIR — Linux's per-user tmpfs at /run/user/$UID.
//      Survives only the user's login session; importantly, it is
//      *not* subject to the systemd-tmpfiles 10-day timer that can
//      scrub /tmp out from under a long-running daemon.
//   2. $TMPDIR — macOS sets this to a per-user dir under
//      /var/folders/...; Linux usually doesn't set it.
//   3. /tmp — universal fallback. The lockfile name carries the uid
//      below, so cross-user collision in /tmp is impossible.
//
// We don't mkdir anything. We assume the chosen directory already
// exists (it always does, for all three options above).
std::string runtime_dir() {
  for (const char *var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
    const char *v = std::getenv(var);
    if (v != nullptr && v[0] != '\0') return v;
  }
  return "/tmp";
}

// Build the absolute lockfile path:
//   <run-dir>/beethoven-<uid>-<key>.lock
// Embedding the uid keeps /tmp safe across users; embedding the
// project key (FNV of the canonical project root) keeps independent
// projects from contending. The "beethoven-" prefix makes the file
// easy to spot when triaging.
std::string lockfile_path() {
  return runtime_dir() + "/beethoven-" +
         std::to_string(static_cast<unsigned long>(geteuid())) + "-" +
         project_key() + ".lock";
}

// Read the holding daemon's PID out of the lockfile. Diagnostic only:
// the kernel doesn't derive the lock state from this value, and we
// only consult it to print a helpful "stop PID NNN first" message.
// Returns -1 if anything goes wrong; callers must tolerate that.
long read_holding_pid(int fd) {
  char buf[32] = {};
  // pread doesn't move the file offset, which keeps things simple if
  // the caller wants to do other I/O on the fd later.
  ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) return -1;
  return std::strtol(buf, nullptr, 10);
}

}  // namespace

void runtime_acquire_singleton_lock() {
  // Idempotent fast-path: if we've already taken the lock in this
  // process, do nothing. Lets us call this from multiple entry-point
  // hooks (verilator main, vpi startup, fpga main) without bookkeeping.
  if (g_lock_fd >= 0) return;

  const std::string path = lockfile_path();

  // Open the lockfile with O_CREAT (no O_EXCL — concurrent daemons
  // racing through `open` is fine, the lock below is what enforces
  // exclusivity). Permissions 0600 because nobody else has any
  // business reading or writing this user's runtime metadata.
  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0600);
  if (fd < 0) {
    std::cerr << "BeethovenRuntime: cannot open singleton lockfile '"
              << path << "': " << std::strerror(errno) << std::endl;
    std::exit(1);
  }

  // LOCK_EX | LOCK_NB: take an *exclusive* lock if we can do so right
  // now; fail fast otherwise. The lock is held by the kernel and is
  // released automatically when this process exits — no atexit
  // handler or explicit unlock is needed for crash safety. (Note that
  // flock locks are attached to the open file description, so even if
  // we forget to close the fd it will be released at process teardown.)
  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    if (errno == EWOULDBLOCK) {
      // Another live daemon for this project is holding the lock.
      // Print its PID (best-effort) so the user knows what to stop.
      const long pid = read_holding_pid(fd);
      std::cerr << "BeethovenRuntime: another instance is already "
                   "running for this project";
      if (pid > 0) std::cerr << " (PID " << pid << ")";
      std::cerr << ". Stop it first";
      if (pid > 0) std::cerr << " (e.g. `kill " << pid << "`)";
      std::cerr << "." << std::endl;
      std::cerr << "  Lockfile: " << path << std::endl;
    } else {
      std::cerr << "BeethovenRuntime: flock failed on '" << path
                << "': " << std::strerror(errno) << std::endl;
    }
    close(fd);
    std::exit(1);
  }

  // We hold the lock — record our PID for diagnostics. Truncate first
  // so a previous (now-dead) daemon's PID doesn't linger after we
  // crash without rewriting it. Both ftruncate and pwrite are
  // best-effort here: the lock state is *not* derived from file
  // contents, so a write failure is non-fatal — just less helpful
  // when the next operator runs `cat` on the lockfile.
  if (ftruncate(fd, 0) != 0) {
    std::cerr << "BeethovenRuntime: warning: could not truncate "
                 "lockfile '" << path << "': "
              << std::strerror(errno) << std::endl;
  }
  char pidbuf[32];
  int n = std::snprintf(pidbuf, sizeof(pidbuf), "%ld\n",
                        static_cast<long>(getpid()));
  if (n > 0) {
    // glibc's pwrite is `warn_unused_result`; a bare `(void)` cast
    // doesn't silence it on gcc 11+. Capture the return into a
    // throwaway local instead. This write is best-effort — failure
    // just means the lockfile won't carry our PID for diagnostics.
    ssize_t written = pwrite(fd, pidbuf, static_cast<size_t>(n), 0);
    (void)written;
  }

  // Stash the fd so it survives until process exit. See the comment
  // on g_lock_fd above for why this matters.
  g_lock_fd = fd;

  // Stale shmem cleanup. We hold the per-project flock, so we know
  // no other BeethovenRuntime is running for this project. POSIX
  // shmem doesn't auto-clean on process exit, so a previous daemon
  // killed via SIGINT/SIGKILL would leave segments behind whose
  // internal pthread mutexes are in an undefined state. Any new
  // daemon (us) trying to re-init those mutexes via
  // pthread_mutex_init would either silently succeed or hang on
  // first lock; either way the testbench eventually deadlocks.
  // Unlinking the names now means data_server_f / cmd_server_f's
  // upcoming shm_open(O_CREAT | O_RDWR) calls will create fresh,
  // pristine segments.
  //
  // Caveat: shmem names are per-user (`/compo_c_<uid>`,
  // `/compo_d_<uid>`), NOT per-project. If another project's daemon
  // happens to be running for this same user, this unlink will
  // remove its segment names too. That's a pre-existing limitation
  // (multi-project concurrent use is broken at the shmem layer
  // regardless); single-project use — the common case — gets a
  // clean restart. shm_unlink returns ENOENT on first run after
  // install; we ignore the return value either way.
  shm_unlink(beethoven::cmd_server_file_name().c_str());
  shm_unlink(beethoven::data_server_file_name().c_str());
}

}  // namespace beethoven
