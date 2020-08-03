# Tina
Tina is a teeny tiny, header only, coroutine and job library!

## Features:
* Super simple API: Basically just `init()` and `yield()`.
* Bring your own memory (or let Tina `malloc()` for you).
* Fast assembly language implementations.
* Supports GCC / Clang with inline assembly, and MSVC with inline machine code.
* Cross platform, supporting most common modern ABIs.
	* SysV for amd64 (Unixes + probably PS4)
	* Win64 (Windows + probably Xbox?)
	* ARM aarch32 and aarch64 (Linux, Rasperry Pi + probably iOS / Android / Switch?)
* Minimal code footprint. Currently ~200 sloc to support many common ABIs.
* Minimal assembly footprint to support a new ABI. (armv7 is like a dozen instructions)

## Limitations:
* Limited testing: User feedback has been good, and they "work on my machines". (More feedback please. :D)
* Supported platforms are the ones I've used recently. Probably needs #ifdef changes for more exotic ones. (Pull requests welcome.)
* I don't personally care about old or less common ABIs, for example: 32 bit Intel, MIPS, etc. (Pull requests welcome.)
* No WASM support. Stack manipulation is intentionally disallowed in WASM for now, and the workarounds are complicated.
* Limited stack overflow detection! Bring your own memory means you need to bring your own guard pages and security.
* Not fully symmetric: Coroutines can call other coroutines, but you can't call a coroutine that hasn't yielded back to it's caller.

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
