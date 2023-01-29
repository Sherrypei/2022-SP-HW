// reference B10902062

#include "status.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>


int fifo[] = {-1, 14, -1, 10, 13, -1, 8, 11, 9, 12};
int targetid[] = {6, 6, 7, 7, 8, 8, 9, 9, 12, 12, 10, 13, 13, 11, 2};


void read_Status(int fifo_fd, Status *ptr, int player_id, int parent_pid, bool pipe, bool from, int target_pid) {

    char c[1024];
    memset(c, 0, sizeof(c));

    if (pipe) {
        if (from) {//[self's ID],[self's pid] pipe from [target's ID],
            // [target's pid] [real_player_id],[HP],[current_battle_id],[battle_ended_flag]\n
            read(STDIN_FILENO, ptr, sizeof(Status));
            sprintf(c, "log_player%d.txt", ptr->real_player_id);
            FILE *logfp = fopen(c, "a");
            fprintf(logfp, "%d,%d pipe from %c,%d %d,%d,%c,%d\n",
                    player_id, parent_pid,
                    targetid[player_id] + 65, target_pid,
                    ptr->real_player_id, ptr->HP, ptr->current_battle_id, ptr->battle_ended_flag);
            fclose(logfp);

        }
    } else {
        if (from) {//fifo from [self's ID],[self's pid] fifo from [target's id] [real_player_id],[HP]\n
            //8,2007 fifo from 0 0,3\n
            read(fifo_fd, ptr, sizeof(Status));
            sprintf(c, "log_player%d.txt", ptr->real_player_id);
            FILE *logfp = fopen(c, "a");
            fprintf(logfp, "%d,%d fifo from %d %d,%d\n",
                    player_id, parent_pid,
                    ptr->real_player_id,
                    ptr->real_player_id, ptr->HP);
            fclose(logfp);
        }
    }

}

void write_Status(int fifo_fd, Status *ptr, int player_id, int parent_pid, bool pipe, bool from,
                  int target_id, int target_pid) {
    char c[1024];
    if (pipe) {
        if (!from) {
            write(STDOUT_FILENO, ptr, sizeof(Status));
            memset(c, 0, sizeof(c));
            sprintf(c, "log_player%d.txt", ptr->real_player_id);
            FILE *logfp = fopen(c, "a");
            fprintf(logfp, "%d,%d pipe to %c,%d %d,%d,%c,%d\n",
                    player_id, parent_pid,
                    targetid[player_id] + 65, target_pid,
                    ptr->real_player_id, ptr->HP, ptr->current_battle_id, ptr->battle_ended_flag);
            fclose(logfp);
        }
    } else {
        if (!from) {
            write(fifo_fd, ptr, sizeof(Status));
            memset(c, 0, sizeof(c));
            sprintf(c, "log_player%d.txt", ptr->real_player_id);
            FILE *logfp = fopen(c, "a");
            fprintf(logfp, "%d,%d fifo to %d %d,%d\n",
                    player_id, parent_pid,
                    target_id,
                    ptr->real_player_id, ptr->HP);
            fclose(logfp);
        }
    }

}


int main(int argc, const char **argv) {
    signed int i;
    int player_id;
    int parent_pid;
    int fifo_fd;
    int openfifo_fd;
    int original_HP;
    FILE *stream, *logfp;
    char c[1024];
    char path[1024];
    char s1[1024];
    char buf[1032];
    player_id = atoi(argv[1]);// int to char player id
    parent_pid = atoi(argv[2]);// parent id
    int pid = getpid();
    Status player_status;
    Status *ptr = &player_status;
    if (player_id > 7)// agent player read form real player
    {
        memset(path, 0, sizeof(path));
        sprintf(path, "./player%d.fifo", player_id);
        mkfifo(path, 0666);
        fifo_fd = open(path, O_RDONLY);
        read_Status(fifo_fd, ptr, player_id, pid, 0/*pipe*/, 1/*from*/,
                    -1/*target_pid  no need to output in fifo*/);
        close(fifo_fd);
        write_Status(-1, ptr, player_id, pid, 1, 0,
                     targetid[player_id] + 65, parent_pid);

    } else// real player
    {
        ptr->real_player_id = player_id;
        memset(c, 0, sizeof(c));
        sprintf(c, "log_player%d.txt", player_id);
        logfp = fopen(c, "a");
        stream = fopen("./player_status.txt", "r");
        for (i = 0; i <= ptr->real_player_id; ++i) {//read status.txt
            fgets(buf, 100, stream);
            // ptr is struct
            sscanf(buf, "%d %d %s %c %d\n",
                   &ptr->HP,
                   &ptr->ATK,
                   s1,
                   &ptr->current_battle_id,
                   &ptr->battle_ended_flag);
        }
        fclose(stream);
        if (!strcmp(s1, "GRASS"))// attr
        {
            ptr->attr = 1;
        } else if (!strcmp(s1, "WATER")) {
            ptr->attr = 2;
        } else if (!strcmp(s1, "FIRE")) {
            ptr->attr = 0;
        }
        //pipe to parent
        write_Status(-1, ptr, player_id, pid, 1, 0,
                     ptr->current_battle_id, parent_pid);

    }
    original_HP = ptr->HP;// original HP
    while (1) {
        while (1) {
            //pipe from parent battle
            read_Status(0, ptr, player_id, pid, 1, 1, parent_pid);//pipe from parent
            if (ptr->battle_ended_flag == 1)
                break;
            //pipe to parent battle
            write_Status(-1, ptr, player_id, pid, 1, 0,
                         -1, parent_pid);
        }
        if (ptr->HP <= 0)// loser
            break;
        if (ptr->current_battle_id == 65)// current battle id
            exit(0);
        ptr->battle_ended_flag = 0;//winner keep fighting
        ptr->HP = (((original_HP - ptr->HP) / 2) + ptr->HP);
        //winner pipe to parent battle
        write_Status(-1, ptr, player_id, pid, 1, 0,
                     -1, parent_pid);
    }
    //player loser
    if ((ptr->current_battle_id != 65) && player_id <= 7) {
        memset(path, 0, sizeof(path));
        sprintf(path, "./player%d.fifo", fifo[ptr->current_battle_id - 65]);
        ptr->HP = original_HP;
        ptr->battle_ended_flag = 0;
        mkfifo(path, 0666);
        openfifo_fd = open(path, O_WRONLY);
        write_Status(openfifo_fd, ptr, player_id, pid, 0, 0,
                     fifo[ptr->current_battle_id - 65], parent_pid);
        close(openfifo_fd);
    }
    fclose(logfp);
    exit(0);
}