# Tina
Tina is a teeny tiny, header only, coroutine and fiber library!

Coroutines are little lightweight userspace threading primitives, and they are super handy. OS threads are great if you want to use multiple CPUs, but syncronizing them is tricky and cumbersome. If you just want to run more than one bit of code at a time, coroutines are much easier to use. This makes them great for lightweight uses like implementing state machines directly as easy to read code, running a script in a video game to control a cutscene, or amortizing the cost of an long algorithm over time. Unlike OS threads, having thousands or millions of coroutines is not a problem. You are really only limited by RAM.

## Tina is feature packed!
* Super simple API: Basically just `init()`, `resume()` and `yield()` for assymetric coroutines
* Fully symmetric coroutines (fibers) are supported too!
* Bring your own memory (or let Tina `malloc()` for you)
* Fast assembly language implementations
* Cross platform, supporting several of the most common modern ABIs
	* System V for amd64: Mac, Linux, BSD, etc (and probably PS4)
	* Win64: Windows (and probably Xbox)
	* ARM aarch32 and aarch64: Unixes (and probably Mac / iOS / Android / Switch)
* Supports GCC / Clang using inline assembly, and MSVC using embedded machine code
* Minimal assembly footprint required to support a new ABI (armv7 is like a dozen instructions)
* Minimal code footprint. Currently ~200 sloc

## Limitations:
* Obsolete or uncommon ABIs aren't supported (ex: 32 bit x86, MIPS, etc. Pull requests are welcome)
* WASM explicitly forbids stackful coroutines :(
* No RISCV support... yet ;)
* Minimal built-in stack overflow protection: Bring your own memory means you need to bring your own guard pages and security too

# Tina Jobs
Tina Jobs is a simple fiber based job system built on top of Tina. (Based on the ideas here: https://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine)

## Tina Jobs Features:
* Jobs may yield to other jobs or abort before they finish. Each is run on a separate coroutine
* Bring your own memory and threading
* No dynamic allocations required at runtime
* Multiple queues: You control when to run them and how
	* Parallel queues: Run a single queue from many worker threads
	* Serial queues: Run a queue from a single thread or poll it from somewhere
* Simple priority model by linking queues together
* Queue switching allows moving a job between queues
	* Ex: Load a texture on a parallel worker thread, but submit it on a serial graphics thread
* Reasonable throughput: Though not a primary goal, even a Raspberry Pi can handle millions of jobs/sec!
* Minimal code footprint: Currently ~300 sloc, which should make it easy to modify and extend

## Limitations:
* Not designed for extreme concurrency or throughput 
	* Single lock per scheduler, doesn't implement work stealing, etc.
* No dynamic allocations at runtime means you have to cap the maximum job/fiber counts at init time
* API stability: I'm still making occasional changes and simplifications

# What are coroutines anyway?

Functions are a simple and useful concept in structured programming. You give them some data, they process it, and return some data back to their caller. Coroutines on the other hand _yield_ instead of returning, and can be _resumed_ so they continue right where they left off.

There is a lot of confusing terminology around threading. So here's my best attempt at clarifying some of it.
* **Thread:** A thread of execution. A flow of instructions as they are executed and their state (ex: CPU registers + stack).
* **Hardware Thread:** The hardware pipeline that actually executes a thread. Usually a CPU core, but features like hyperthreading can provide multiple hardware threads per core.
* **OS Thread:** OS threads are usually what people mean when simply saying "thread". It's a thread that is scheduled and managed by the OS. Usually using CPU interrupts to switch threads automatically without requiring any code changes to support multi-tasking. (ex: Windows threads, pthreads, etc)
* **Fiber:** A lightweight thread implemented in user space code. Much simpler and faster than OS threads, but the fiber must explicitly yield to other fibers. (ex: Windows fibers, POSIX contexts, Tina coroutines)

So what's the difference between coroutines, fibers, generators, continuations, contexts, etc? Well... not much, and also a lot depending on who you talk to. Many aren't rigorously defined, so they tend to be used interchangeably. Some implementations operate at a language level by saving local variables. Some work by saving CPU registers. Some implementations have their own stack, while others work only within a single stack frame. Some implementations are asymmetric and can only yield back to the coroutine that resumed them, while others are symmetric and can switch to any other coroutine arbitrarily. Sometimes the terminology is simply a matter of what it's used for. For example generators are basically coroutines used like iterators.

Tina's coroutines (or fibers, or whatever you want to call them) each have their own stack, and they work by using ABI specific assembly code to save and restore the CPU registers. They can also be used in either a symmetric or asymmetric fashion which is handy. There are other coroutine/fiber libraries that provide a fast assembly implementation of course, ~~but as far as I know Tina is the only one with a simple header only implementation~~ (1). I'm not always a huge fan of header only libs, but avoiding a mess of assembler files in a cross platform project is quite nice! By supporting a few of the most common ABIs, Tina should run on all of the current desktop, mobile, and console platforms available in 2021. \o/

(1) Here's a new library that is very similar to Tina: https://github.com/edubart/minicoro
