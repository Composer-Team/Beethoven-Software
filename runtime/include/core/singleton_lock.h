//
// singleton_lock.h
//
// Per-project singleton lock for BeethovenRuntime.
//
// At most one BeethovenRuntime daemon may run for a given project at a
// time. This module acquires an OS-level exclusive lock keyed on the
// canonicalized BEETHOVEN_PROJECT_ROOT, so the same machine can host
// many independent project daemons concurrently — what's exclusive is
// the (user, project) pair, not the BeethovenRuntime binary.
//
// Crash-safe: the lock is held by the kernel, not by file contents,
// so SIGKILL / segfault / OOM all release it automatically. There is
// no stale-lock cleanup path; an abruptly-killed daemon leaves no
// residue that needs sweeping.
//
// See Beethoven-Software/cli/docs/runtime.md (Concurrency section) for
// the design and the matching CLI-side probe.
//

#ifndef BEETHOVEN_RUNTIME_SINGLETON_LOCK_H
#define BEETHOVEN_RUNTIME_SINGLETON_LOCK_H

namespace beethoven {

// Acquire the per-project singleton lock for BeethovenRuntime.
//
// Idempotent: subsequent calls within the same process are silent
// no-ops once the lock has been acquired (so wiring this into more
// than one entry-point hook is harmless).
//
// On failure — lock held by another live daemon for this project, or
// any unexpected I/O error — prints a diagnostic to stderr (including
// the holding PID, where readable) and exits the process with status
// 1. This is intentional: it's strictly less surprising for the
// daemon to refuse to start than to start in a state where it might
// race with another instance.
//
// On success the lock is held by the kernel for the lifetime of the
// process. The kernel auto-releases on exit (clean or abrupt), so no
// explicit release is required and no atexit handler is registered.
//
// Requires the BEETHOVEN_PROJECT_ROOT environment variable to be set
// to the project root (the directory containing Beethoven.toml). The
// CLI sets this when launching the daemon; if running by hand, set it
// explicitly. Refuses to start if it is unset or empty.
void runtime_acquire_singleton_lock();

}  // namespace beethoven

#endif  // BEETHOVEN_RUNTIME_SINGLETON_LOCK_H
