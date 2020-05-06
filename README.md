# Tina
Tina is a teeny tiny, header only, coroutine and job library!

## Features:
* Super simple API. Basically just init() and yield().
* Fast assembly language implementations.
* Bring your own memory allocator.
* Supports GCC / Clang with inline assembly, and MSVC with inline machine code.
* Cross platform, supporting most common modern ABIs.
	* SysV for amd64 (Unixes + probably PS4)
	* Win64 (Windows + probably Xbox?)
	* ARM aarch32 and aarch64 (Linux, Rasperry Pi + probably iOS / Android / Switch?)
* Minimal code footprint. Currently ~200 sloc to support many common ABIs.
* Minimal assembly footprint to support a new ABI. (armv7 is like a dozen instructions)

## Non-Features:
* Maybe-not-quite production ready. (Please help me test!)
* #ifdef checks for every concievable system/compiler on the supported ABIs. (Pull requests encouraged!)
* I don't personally care about old or less common ABIs, for example: 32 bit Intel, MIPS, etc. (Pull requests welcome.)
* No WASM support. Stack manipulation is intentionally disallowed in WASM for now, and the workarounds are far from ideal.
* Not vanilla, "portable", C code by wrapping kinda-sorta-deprecated, platform specific APIs like `CreateFiber()` or `makecontext()`.
* No stack overflow protection. Memory, and therefore memory protection is the user's job.

# Tina Jobs
Tina Jobs is a job system built on top of Tina.

Based on this talk: https://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine (Everyone else seems to love this, and so do I. <3)

## Features:
* Simple API. Basically just init() / equeue() / wait() + convenience functions.
* Jobss may yield to other jobs or abort before they finish. Each is run on it's own fiber backed by a tina coroutine.
* Bring your own memory allocator and threading.
* Supports multiple queues, and you can decide when to run them and how.
	* If you want a parallel queue for computation, run it from many worker threads.
	* If you want a serial queue, poll it from a single thread or job.
* Queue priorities allows implementing a simple job priority model.
* Flexible wait primitive allows joining on all subjobs, or throttling a multiple producer / consumer system.
* Minimal code footprint. Currently ~300 sloc, should make it easy to modify.

## Non-Features:
* Not lock free: Atomics are hard...
* Not designed for extreme concurrency or performance.
	* Not lock free, doesn't implement work stealing, etc.
	* Even my Raspberry Pi 4 had over 1 million jobs/sec for throughput. So it's not bad either.
