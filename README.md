# Tina
Tina is a teeny tiny, header only, coroutine and job library!

## Features:
* Super simple API: Basically just `init()` and `yield()`.
* Bring your own memory (or let Tina `malloc()` for you).
* Fast assembly language implementations.
* Supports GCC / Clang with inline assembly, and MSVC with inline machine code.
* Cross platform, supporting several of the most common modern ABIs.
	* SysV for amd64 (Unixes + probably PS4)
	* Win64 (Windows + probably Xbox?)
	* ARM aarch32 and aarch64 (Linux, Rasperry Pi + probably iOS / Android / Switch?)
* Minimal assembly footprint to support a new ABI. (armv7 is like a dozen instructions)
* Minimal code footprint. Currently ~200 sloc.

## Limitations:
* Limited testing: User feedback has been good, and they "work on my machines". (More feedback please. :D)
* Probably needs ifdef changes on platforms I didn't test on, like consoles or mobile. (Pull requests welcome.)
* I don't personally care about old or less common ABIs, for example: 32 bit Intel, MIPS, etc. (Pull requests welcome.)
* No WASM support. Stack manipulation is intentionally disallowed in WASM for now, and the workarounds are complicated.
* Limited stack overflow detection! Bring your own memory means you need to bring your own guard pages and security.
* Probably not secure: Provides limited stack overflow detection. Bring your own guard pages and security if needed.

# Tina Jobs
Tina Jobs is a fiber based job system built on top of Tina.

Based on this talk: https://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine (Everyone else seems to love this, and so do I. <3)

## Features:
* Simple API. Basically just init() / equeue() / wait().
* Jobs may yield to other jobs or abort before they finish. Each is run on it's own fiber backed by a tina coroutine.
* Bring your own memory and threading.
* No dynamic allocations at runtime.
* Supports multiple queues, and you can decide when to run them and how.
	* If you want a parallel queue for computation, run it from many worker threads.
	* If you want a serial queue, run or poll it from a single place (thread or job).
* Queue priorities allows implementing a simple job priority model.
* Queue switching allows moving a job between queues. (Ex: Load a texture on a parallel worker thread, but submit it on a serial graphics thread)
* Simple, but flexible wait primitive allows joining on one or more subjobs, or even throttling a multiple producer / consumer system.
* Reasonable throughput: Though it wasn't a primary goal, even a Raspberry Pi 4 can handle over a million jobs/sec.
* Minimal code footprint. Currently ~300 sloc, should make it easy to modify.

## Limitations:
* Not designed for extreme concurrency or throughput. (Not lock free, doesn't implement work stealing, etc)
* No dynamic allocations at runtime means you have to cap the maximum job/fiber counts at init time.
* Limited built in syncronization: No a trivial way for multiple jobs to wait for the same job.
