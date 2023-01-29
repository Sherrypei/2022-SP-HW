#ifndef THREADTOOL
#define THREADTOOL

#include <setjmp.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    char fifoname[BUF_SIZE];
    int i, x, y;  // declare the variables you wish to keep between switches
};

extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);

void scheduler();

#define thread_create(func, id, arg) { \
           func(id, arg);                            \
}

#define thread_setup(id, arg) { \
struct tcb *tmp = (struct tcb *)malloc(sizeof (struct tcb));                         \
        tmp->id = id;           \
        tmp->arg = arg;\
sprintf(tmp->fifoname, "%d_%s", id, __func__);                 \
        mkfifo(tmp->fifoname, 0666);                        \
        tmp->fd = open(tmp->fifoname, O_RDONLY | O_NONBLOCK );                        \
        if (setjmp(tmp->environment) == 0){                                   \
        ready_queue[rq_size] = tmp;           \
                             rq_size++;             \
        return;                     \
        }                       \
        \
                                \
                                \
\
}

#define thread_exit() { \
         close(ready_queue[rq_current]->fd);               \
            unlink(ready_queue[rq_current]->fifoname);            \
          longjmp(sched_buf, 3);              \
}

#define thread_yield() { \
        if (setjmp(ready_queue[rq_current]->environment) == 0){                                   \
             sigprocmask(SIG_SETMASK, &alrm_mask, NULL);                                          \
             sigprocmask(SIG_SETMASK, &tstp_mask, NULL);                                          \
             sigprocmask(SIG_SETMASK, &base_mask, NULL);\
        }\
}

#define async_read(count) ({ \
      if (setjmp(ready_queue[rq_current]->environment) == 0){                        \
            longjmp(sched_buf, 2) ;                  \
                             \
          }                   \
      else {                 \
         read(ready_queue[rq_current]->fd, ready_queue[rq_current]->buf, count);                    \
      \
      }                       \
})

#endif // THREADTOOL
