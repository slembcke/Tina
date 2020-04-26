#include "libs/tinycthread.h"

typedef struct tina_scheduler tina_scheduler;

void common_start_worker_threads(unsigned thread_count, tina_scheduler* sched, unsigned queue_idx);
void common_destroy_worker_threads();
