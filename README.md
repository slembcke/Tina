# Tina
Tina is a teeny tiny, header only, coroutine/fiber library!

## Features:
* Super simple API: Basically just `init()` and `swap()` for symmetric coroutines
* Asymmetric coroutines are supported too via `resume()` and `yield()`
* Bring your own memory (or let Tina `malloc()` for you)
* Fast assembly language implementations
* Cross platform, supporting several of the most common modern ABIs
	* System V for amd64 (Unixes and probably PS4)
	* Win64 (Windows and probably Xbox)
	* ARM aarch32 and aarch64 (Unixes, and probably iOS / Android / Switch)
* Supports GCC / Clang using inline assembly, and MSVC using embedded machine code
* Minimal assembly footprint to support a new ABI (armv7 is like a dozen instructions)
* Minimal code footprint. Currently ~200 sloc

## Limitations:
* I don't personally care about old or less common ABIs, for example: 32 bit Intel, MIPS, etc. (Pull requests welcome)
* No WASM support: The stack is private and hidden by the WASM runtime so it's not possible to make stackful coroutines.
* Minimal stack overflow detection: Bring your own memory means you need to bring your own guard pages and security.

# Tina Jobs
Tina Jobs is a fiber based job system built on top of Tina. (Based on the ideas here: https://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine)

## Features:
* Pretty simple API: Basically just `init()`, `equeue()`, and `wait()`
* Jobs may yield to other jobs or abort before they finish. Each is run on a separate tina coroutine.
* Bring your own memory and threading.
* No dynamic allocations at runtime.
* Supports multiple queues: You control when to run them and how.
	* If you want a parallel queue for computation, run it from many worker threads.
	* If you want a serial queue, run or poll it from a single thread (or job).
* Simple priority model by chaining queues together.
* Queue switching allows moving a job between queues. (Ex: Load a texture on a parallel worker thread, but submit it on a serial graphics thread)
* Reasonable throughput: Though it wasn't a primary goal, even a Raspberry Pi 4 can handle millions of jobs/sec.
* Minimal code footprint: Currently ~300 sloc, which should make it easy to modify and extend.

## Limitations:
* Not designed for extreme concurrency or throughput. (Single lock per scheduler, doesn't implement work stealing, etc)
* No dynamic allocations at runtime means you have to cap the maximum job/fiber counts at init time.
* Limited built in syncronization: Ex: There isn't a trivial way for multiple jobs to wait for the same job.
* I'm still tinkering with the jobs API for my own needs. You should too!

# What are coroutines anyway?

Functions are a simple and useful concept in structured programming. You give them some data, they process it, and return some data back to their caller. Coroutines on the other hand _yield_ instead of returning, and can be _resumed_ so they continue right where they left off. Coroutines are neat because they can directly and compactly implement state machines (like for a communication protocol), or allow code to run over time (ex: amortize the cost of an algorithm in a game a little each frame).

There is a lot of confusing terminology around threading. Here's my best attempt at clarifying some of it.
* **Thread:** A thread of execution. A flow of instructions as they are executed and their state (ex: CPU registers + stack).
* **Hardware Thread:** Hardware that provides the execution of a thread. Usually a CPU core, but features like hyperthreading can provide multiple hardware threads per core.
* **OS Thread:** A thread that is scheduled and managed by the OS. Usually using CPU interrupts to switch threads without requiring any help from the thread itself. (ex: Windows threads, pthreads) OS threads are usually what people mean when simply saying "thread".
* **Fiber:** A lightweight thread in user space code. Much simpler and faster than OS threads, but the fiber must explicitly yield to other fibers. (ex: Windows fibers, POSIX contexts, Tina coroutines)

So what's the difference between coroutines, fibers, generators, continuations, contexts, etc? Well... not much, and also a lot depending on who you talk to. Many aren't rigorously defined, so they tend to be used interchangeably. Some implementations operate at a language level by preserving local variables. Some work by saving CPU registers. Some implementations have their own stack, while others work only within a single stack frame. Some implementations are asymmetric and can only yield back to the coroutine that resumed them, while others are symmetric and can switch to any other coroutine arbitrarily. Sometimes the terminology is simply a matter of what it's used for. For example generators are basically coroutines used like iterators.

Tina's coroutines (or fibers, or whatever you want to call them) each have their own stack, and they work by using platform specific assembly code to save and restore the CPU registers. They can also be used in either a symmetric or asymmetric fashion which is handy. There are other coroutine/fiber libraries that can provides a fast assembly implementation of course, but as far as I know Tina is the only one with a simple header only implementation. I'm not always a huge fan of header only libs, but it avoids dealing with assembler files in a cross platform. By supporting the common 64 bit x86 and 32/64 bit ARM ABIs Tina should run on all of the current desktop, mobile, and console platforms available in 2020.
