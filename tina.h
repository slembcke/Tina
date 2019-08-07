#include <inttypes.h>

typedef struct tina tina;
typedef uintptr_t tina_func(tina* coro, uintptr_t value);
typedef void tina_err_func(void);

tina_err_func* tina_err;

struct tina {
	void* ctx;
	void* _rsp;
	uint8_t _stack[];
};

uintptr_t tina_wrap(tina* coro, uintptr_t value);
uintptr_t tina_resume(tina* coro, uintptr_t value);
uintptr_t tina_yield(tina* coro, uintptr_t value);
