# Tina
Tina is a teeny tiny, header only, coroutine and job library!

## Features:
* Super simple API. Basically just init() and yield().
* Fast assembly language implementations.
* Bring your own memory allocator.
* Supports GCC / Clang with inline assembly, and MSVC with inline machine code.
* Cross platform, supporting most common modern ABIs.
	* SysV for amd64 (Unixes + maybe PS4)
	* Win64 (Windows + maybe Xbox?)
	* ARM aarch32 and aarch64 (Linux, Rasperry Pi + maybe iOS / Android / Switch?)
* Minimal code footprint. Currently ~200 sloc to support many common ABIs.
* Minimal assembly footprint to support a new ABI. (armv7 is like a dozen instructions)

## Non-Features:
* Definitely not production ready! (Please help me test!)
* No intention to support old or less common ABIS, for example: 32 bit Intel, MIPS, etc. Pull requests are welcome though.
* No WASM support. Stack manipulation is intentionally disallowed in WASM for now, and the workarounds seem dumb.
* Not vanilla, "portable", C code by wrapping kinda-sorta-deprecated, platform specific APIs like CreateFiber() or makecontext().
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
* Not designed for high concurrency or performance
	* Not lock free, doesn't implement work stealing, etc.
	* Even my Raspberry Pi 4 had over 1 million jobs/sec for throughput. So it's not bad either.

## Example:
```C
#include <stdlib.h>
#include <stdio.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

static uintptr_t coro_body(tina* coro, uintptr_t value);
static void coro_error(tina* coro, const char* message);

int main(int argc, const char *argv[]){
	// Initialize a coroutine with some stack space, a body function, and some user data.
	uint8_t buffer[1024*1024];
	void* user_data = "some user data";
	tina* coro = tina_init(buffer, sizeof(buffer), coro_body, user_data);
	
	// Optionally set some debugging values.
	coro->name = "MyCoro";
	coro->error_handler = coro_error;
	
	// Call tina_yield() to switch coroutines.
	// You can optionally pass a value through to the coroutine as well.
	while(coro->running) tina_yield(coro, 0);
	
	printf("Resuming again will call coro_error()\n");
	tina_yield(coro, 0);
	
	return EXIT_SUCCESS;
}

// The body function is pretty straightforward.
// It get's passed the coroutine and the first value passed to tina_yield().
static uintptr_t coro_body(tina* coro, uintptr_t value){
	printf("coro_body() enter\n");
	printf("user_data: '%s'\n", (char*)coro->user_data);
	
	for(unsigned i = 0; i < 3; i++){
		printf("coro_body(): %u\n", i);
		tina_yield(coro, 0);
	}
	
	// The return value is returned from tina_yield() in the caller.
	return 0;
}

static void coro_error(tina* coro, const char* message){
	printf("Tina error (%s): %s\n", coro->name, message);
}
```

## TODO:
* Need some real tests.
