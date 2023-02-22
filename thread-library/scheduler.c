#include "threadtools.h"

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO

    //  // print lines to std out
    //  //If the signal caught is SIGALRM, you should reset the alarm here.
    //  // reset the alarm snd then longjump
    if (signo == SIGTSTP) {
        printf("caught SIGTSTP\n");
    } else if (signo == SIGALRM) {
        printf("caught SIGALRM\n");
        alarm(timeslice);
    }
    longjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    int tmp = setjmp(sched_buf);

    if (tmp == 0) {
        longjmp(ready_queue[0]->environment, 1);
    } else {
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        fd_set working_set, master_set;
        FD_ZERO(&working_set);
        for (int i = 0; i < wq_size; i++) {
            FD_SET(waiting_queue[i]->fd, &working_set);
        }
        if (select(1000, &working_set, NULL, NULL, &timeout) > 0) {
            for (int i = 0; i < wq_size; i++) {
                if (FD_ISSET(waiting_queue[i]->fd, &working_set)) {
                    ready_queue[rq_size] = waiting_queue[i];
                    waiting_queue[i] = NULL;
                    rq_size++;
                }
            }
            int j = 0;
            for (int i = 0; i < wq_size; i++) {
                if (waiting_queue[i] != NULL) {
                    waiting_queue[j] = waiting_queue[i];
                    waiting_queue[i] = NULL;
                    j++;
                }
            }
            wq_size = j;
        }
        // TODO For async_read, move the thread to the end of the waiting queue.
        //For thread_exit, clean up the data structures you allocated for this thread, then remove it from the ready queue.
        if (tmp == 2) {
            waiting_queue[wq_size] = RUNNING;
            wq_size++;

            ready_queue[rq_current] = ready_queue[rq_size - 1];
            ready_queue[rq_size - 1] = NULL;
            rq_size--;
        } else if (tmp == 3) {
            free(ready_queue[rq_current]);
            ready_queue[rq_current] = ready_queue[rq_size - 1];
            ready_queue[rq_size - 1] = NULL;
            rq_size--;
            if (rq_size == 0){

            }
        }
        if (tmp == 1) {
            rq_current = ((rq_current + 1) % rq_size);
        } else if (tmp == 2 || tmp == 3) {
            if (rq_current == rq_size) {
                rq_current = 0;
            }
        }
        if (rq_size == 0) {
            if (wq_size == 0) {
                return;
            } else if (wq_size == 1) {
                int flag = 1;
                FD_ZERO(&master_set);
                for (int i = 0; i < wq_size; i++) {
                    FD_SET(waiting_queue[i]->fd, &master_set);
                }
                while (flag) {
                    working_set = master_set;
                    if (select(1000, &working_set, NULL, NULL, &timeout) > 0) {
                        flag = 0;
                        for (int i = 0; i < wq_size; i++) {
                            if (FD_ISSET(waiting_queue[i]->fd, &working_set)) {
                                ready_queue[rq_size] = waiting_queue[i];
                                waiting_queue[i] = NULL;
                                rq_size++;
                            }
                        }
                        int j = 0;
                        for (int i = 0; i < wq_size; i++) {
                            if (waiting_queue[i] != NULL) {
                                waiting_queue[j] = waiting_queue[i];
                                waiting_queue[i] = NULL;
                                j++;
                            }
                        }
                        wq_size = j;
                        rq_current = 0;
                    }
                }
            }
        }

//        printf("rq_size = %d, wq_size = %d\n",rq_size, wq_size);
        longjmp(RUNNING->environment, 1);
        //TODO For thread_yield, you should execute the next thread in the queue.
        //For async_read and thread_exit, you should execute the thread you used to fill up the hole.
    }



    // TODO
}
