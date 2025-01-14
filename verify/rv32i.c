#define TINA_IMPLEMENTATION
#include "tina.h"

#include <stdio.h>
#include <stdint.h>

// 簡單的協程函式，用來測試切換及整數運算
static void* fiberA(tina* coro, void* val) {
    printf("Fiber A start.\n");

    int32_t x = 10;
    x += 20; // x = 30
    printf("Fiber A integer x = %d\n", x);

    // 使用 tina_yield() 暫停自己
    val = tina_yield(coro, (void*)1);

    x *= 2; // x = 60
    printf("Fiber A after yield, x = %d\n", x);
    printf("Fiber A done.\n");

    return val; // 回傳給最終 resume
}

int main() {
    // 準備一段堆疊給協程
    static char stackA[64 * 1024];

    // 初始化纖程
    tina* coroA = tina_init(stackA, sizeof(stackA), fiberA, NULL);

    // 第一次進入 A
    printf("Main: resume fiber A.\n");
    void* ret = tina_resume(coroA, NULL);
    printf("Main: fiber A yield. ret = %ld\n", (long)ret);

    // 再次進入 A
    printf("Main: resume fiber A again.\n");
    ret = tina_resume(coroA, NULL);
    printf("Main: fiber A ended. ret = %ld\n", (long)ret);

    return 0;
}


