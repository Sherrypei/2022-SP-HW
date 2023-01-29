#include "status.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>


char battle_id;
unsigned int pid;
char *battle_children[] = {"B", "C", "D", "E", "F", "14", "G", "H",
                           "I", "J", "K", "L", "0", "1", "2", "3",
                           "4", "5", "6", "7", "M", "10", "N", "13",
                           "8", "9", "11", "12"};
char *battle_parent[] = {"-1", "A", "A", "B", "B", "C", "D",
                         "D", "E", "E", "F", "F", "K", "L"};
int battle_attr[] = {0, 1, 2, 2, 0, 0, 0, 1, 2, 1, 1, 1, 0, 2},
        num_of_player[] = {0, 0, 1, 0, 0, 0, 2, 2, 2,
                           2, 1, 1, 2, 2};

void read_Status(int pipe_fd, Status *ptr, int targetpid, char *target) {
    char c[1024];
    memset(c, 0, sizeof(c));
    read(pipe_fd, ptr, sizeof(Status));
    sprintf(c, "log_battle%c.txt", battle_id);
    FILE *logfp = fopen(c, "a");
    //[self's ID],[self's pid] pipe from [target's ID],
    // [target's pid] [real_player_id],[HP],[current_battle_id],[battle_ended_flag]\n
    //G,1006 pipe from 0,2000 0,3,G,0\n
    fprintf(logfp, "%c,%d pipe from "
                   "%s,%d "
                   "%d,%d,%c,%d\n",
            battle_id, pid,
            target, targetpid,
            ptr->real_player_id, ptr->HP, ptr->current_battle_id, ptr->battle_ended_flag);
    fclose(logfp);
}

void write_Status(int pipe_fd, Status *ptr, int targetpid, char *target) {
    char c[1024];
    memset(c, 0, sizeof(c));
    write(pipe_fd, ptr, sizeof(Status));
    sprintf(c, "log_battle%c.txt", battle_id);
    FILE *logfp = fopen(c, "a");
    //[self's ID],[self's pid] pipe to [target's ID],
    // [target's pid] [real_player_id],[HP],[current_battle_id],[battle_ended_flag]\n
    //G,1006 pipe to 0,2000 0,3,G,0\n
    fprintf(logfp, "%c,%d pipe to %s,%d %d,%d,%c,%d\n",
            battle_id, pid,
            target, targetpid,
            ptr->real_player_id, ptr->HP, ptr->current_battle_id, ptr->battle_ended_flag);
    fclose(logfp);

}

bool play_an_attack(Status *ptr1, Status *ptr2, unsigned int attr) {
    Status temp1 = *ptr1;

    if (ptr1->attr == attr) {
        temp1.ATK *= 2;
    }

    ptr2->HP -= temp1.ATK;
    if (battle_id == 'G') {
    }
    if (ptr2->HP <= 0) {
        ptr1->battle_ended_flag = 1;
        ptr2->battle_ended_flag = 1;
        return true;
    } else return false;
}

int main(int argc, const char **argv, const char **envp) {

    unsigned int winner;
    int first_attack;
    bool passing_mode;
    unsigned int parent_pid;

    int children_alive;
    unsigned int attr;
    pid_t fork_pid;
    int ipc[14];
    char s[1024];
    char buf1[1024], buf2[1032];
    FILE *logfp;
    Status player_status0;
    Status player_status1;
    Status *ptr[2];
    ptr[0] = &player_status0;
    ptr[1] = &player_status1;
    battle_id = *argv[1];
    parent_pid = atoi(argv[2]);
    pid = getpid();
    children_alive = 2 - num_of_player[battle_id - 65];
    attr = battle_attr[battle_id - 65];

    sprintf(s, "log_battle%c.txt", battle_id);
    logfp = fopen(s, "a");
    for (int i = 0; i <= 1; ++i) {
        pipe(&ipc[2 * i + 6]);
        pipe(&ipc[2 * i + 10]);
        fork_pid = fork();
        ipc[i] = fork_pid;
        if (!fork_pid) {// child
            close(ipc[2 * i + 6]);
            close(ipc[2 * i + 11]);
            dup2(ipc[2 * i + 7], 1);
            dup2(ipc[2 * i + 10], 0);
            close(ipc[2 * i + 7]);
            close(ipc[2 * i + 10]);
            sprintf(buf2, "%d", pid);
            if (i >= children_alive) {
                sprintf(buf1, "%s", battle_children[2 * battle_id - 130 + i]);
                execl("./player", "./player", buf1, buf2, STDIN_FILENO);
            } else {
                sprintf(buf1, "%s", battle_children[2 * battle_id - 130 + i]);
                execl("./battle", "./battle", buf1, buf2, STDIN_FILENO);
            }
        }
        close(ipc[2 * i + 7]);
        close(ipc[2 * i + 10]);
        ipc[i + 2] = ipc[2 * i + 6];
        ipc[i + 4] = ipc[2 * i + 11];
    }
    for (int j = 0; j <= 1; ++j)
        read_Status(
                ipc[j + 2], ptr[j],
                ipc[j], battle_children[2 * battle_id - 130 + j]);
    while (!ptr[0]->battle_ended_flag) {
        ptr[0]->current_battle_id = battle_id;
        ptr[1]->current_battle_id = battle_id;
        first_attack = ((ptr[0]->HP) > (ptr[1]->HP)
                        || (ptr[0]->HP == ptr[1]->HP && ptr[0]->real_player_id > ptr[1]->real_player_id));
        if (!play_an_attack(ptr[first_attack], ptr[1 - first_attack], attr)) {
            first_attack = 1 - first_attack;
            play_an_attack(ptr[first_attack], ptr[1 - first_attack], attr);
        }
        for (int k = 0; k <= 1; ++k)
            //int pipe_fd, Status *ptr, char current_battle_id,
            //char *children, int targetpid, char *target
            write_Status(
                    ipc[k + 4], ptr[k],
                    ipc[k], battle_children[2 * battle_id - 130 + k]);
        if (!ptr[0]->battle_ended_flag) {
            for (int m = 0; m <= 1; ++m)
                read_Status(
                        ipc[m + 2], ptr[m],
                        ipc[m], battle_children[2 * battle_id - 130 + m]);
        }
    }
    wait(NULL);
    passing_mode = true;
    while (passing_mode) {
        if (battle_id == 65) {
            winner = ptr[first_attack]->real_player_id;

            wait(NULL);
            passing_mode = false;
        } else {
            read_Status(
                    ipc[first_attack + 2],
                    ptr[first_attack],
                    ipc[first_attack], battle_children[2 * battle_id - 130 + first_attack]);
            write_Status(STDOUT_FILENO, ptr[first_attack],
                         parent_pid, battle_parent[battle_id - 65]);
            read_Status(STDIN_FILENO, ptr[first_attack],
                        parent_pid, battle_parent[battle_id - 65]);
            write_Status(
                    ipc[first_attack + 4],
                    ptr[first_attack],
                    ipc[first_attack], battle_children[2 * battle_id - 130 + first_attack]);
            if (battle_id == 'C')
            {

            }

            if (ptr[first_attack]->HP <= 0 || ptr[first_attack]->current_battle_id == 65 && ptr[first_attack]->battle_ended_flag == 1) {
                wait(NULL);
                passing_mode = false;

            }
        }
    }

    if (battle_id == 65) {
        printf("Champion is P%d\n", winner);
        fflush(stdout);
    }
    fclose(logfp);
    for (int n = 0; n <= 1; ++n) {
        close(ipc[n + 2]);
        close(ipc[n + 4]);
    }
    exit(0);
}