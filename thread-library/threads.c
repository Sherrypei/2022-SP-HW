#include "threadtools.h"

void fibonacci(int id, int arg) {

    thread_setup(id, arg);

    for (RUNNING->i = 1;; RUNNING->i++) {

        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            int tmp = RUNNING->y;
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }

        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void collatz(int id, int arg) {
    // TODO
    // // If N is even, divide it by 2.
    // // If N is odd, multiply it by 3, then add 1 to it.
    thread_setup(id, arg);
    for (RUNNING->i = 1;; RUNNING->i++) {
        if ((RUNNING->arg % 2) == 0) {
            RUNNING->arg = RUNNING->arg / 2;
            printf("%d %d\n", RUNNING->id, RUNNING->arg);
        } else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            RUNNING->arg = RUNNING->arg * 3 + 1;
            printf("%d %d\n", RUNNING->id, RUNNING->arg);
        }
        sleep(1);

        if (RUNNING->arg == 1) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void max_subarray(int id, int arg) {
    // TODO

    thread_setup(id, arg);

    for (RUNNING->i = 1;; RUNNING->i++) {
        async_read(5);
        int tmp = atoi(RUNNING->buf);
        if (RUNNING->i == 1){
            RUNNING->y = tmp;
            if (tmp < 0)RUNNING->x = 0;
            else RUNNING->x = tmp;
        }
        else {
            if (RUNNING->y>0) {
                RUNNING->y += tmp;

            }
            else RUNNING->y = tmp;
            if (RUNNING->x < RUNNING->y){
                RUNNING->x = RUNNING->y;
            }
        }
        printf("%d %d\n", RUNNING->id, RUNNING->x);
        sleep(1);
        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}
