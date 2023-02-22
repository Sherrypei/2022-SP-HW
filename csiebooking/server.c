// #define READ_SERVER

#include <unistd.h>
//#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <netdb.h>
#include <ctype.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define RECORD_PATH "./bookingRecord"


typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    bool wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request *requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

//const char *accept_read_header = "ACCEPT_FROM_READ";
//const char *accept_write_header = "ACCEPT_FROM_WRITE";
const char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request *reqP);
// initailize a request instance

static void free_request(request *reqP);
// free resources used by a request instance

typedef struct {
    int id;          // 902001-902020
    int bookingState[3]; // array of booking number of each object (0 or positive numbers)
} record;
record temp;
bool writelock[23];
/*bool samewl[23];
int samerl[23];*/
int readlock[23];

int handle_read(request *reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512];

    memset(buf, 0, sizeof(buf));
    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char *p1 = strstr(buf, "\015\012");
    if (p1 == NULL) {
        p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len - 1;
//    fprintf(stderr, "handle = '%s'", reqP->buf);
    return 1;
}

void endConnection(int conn_fd, fd_set *set) {
    FD_CLR(conn_fd, set);
    close(requestP[conn_fd].conn_fd);
    free_request(&requestP[conn_fd]);
}

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
    struct flock lock;
    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;
    return (fcntl(fd, cmd, &lock));
}

void unlock(int file_fd, int conn_fd) {
    lock_reg(file_fd, F_SETLK, F_UNLCK, sizeof(record) * (requestP[conn_fd].id - 1),
             SEEK_SET,
             sizeof(record));
}

void setreadlock(int file_fd, int conn_fd) {
    lock_reg(file_fd, F_SETLK, F_RDLCK, sizeof(record) * (requestP[conn_fd].id - 1),
             SEEK_SET,
             sizeof(record));
    readlock[requestP[conn_fd].id]++;
}

void setwritelock(int file_fd, int conn_fd) {
    lock_reg(file_fd, F_SETLK, F_WRLCK, sizeof(record) * (requestP[conn_fd].id - 1),
             SEEK_SET,
             sizeof(record));
    writelock[requestP[conn_fd].id] = true;
}

int main(int argc, char **argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // file_fd for a new connection with client
    int file_fd = open(RECORD_PATH, O_RDWR);  // file_fd for file that we open for reading
    char buf[512];
//    int buf_len;
    int ret;


    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, file_fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd,
            maxfd);

    fd_set working_set, master_set;
    FD_ZERO(&master_set);
    FD_SET(svr.listen_fd, &master_set);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1;
    while (1) {
        working_set = master_set;
        if (select(maxfd, &working_set, NULL, NULL, &timeout) <= 0)
            continue;
        if (FD_ISSET(svr.listen_fd, &working_set)) {//listen file_fd
            // Check new connection
            clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }
            requestP[conn_fd].conn_fd = conn_fd;
            FD_SET(conn_fd, &master_set);
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... file_fd %d from %s\n", conn_fd, requestP[conn_fd].host);
            sprintf(buf, "Please enter your id (to check your booking state):\n");
            write(requestP[conn_fd].conn_fd, buf, strlen(buf));
            continue;
        }
        //already have connection
        for (int i = 3; i < maxfd; i++) {
//                fprintf(stderr, "test0\n");
//            int id = requestP[conn_fd].id % 100;
//            fprintf(stderr, "i = %d, isset = %d\n", i, FD_ISSET(i, &working_set));
            if (FD_ISSET(i, &working_set)) {
//                fprintf(stderr, "test1\n");
                conn_fd = i;
                if (!requestP[conn_fd].wait_for_write) {//enter id
//                    fprintf(stderr, "test2\n");
                    int id;
                    ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
//                    fprintf(stderr, "ret = %d\n", ret);
                    if (ret <= 0) {
                        endConnection(conn_fd, &master_set);
                        break;
                    }
                    bool valid = true;
                    // check valid id
                    for (int j = 0; j < strlen(requestP[conn_fd].buf); j++) {
                        if (isdigit(requestP[conn_fd].buf[j])) {
                            valid = true;
                        } else {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        requestP[conn_fd].id = atoi(requestP[conn_fd].buf);
                        id = requestP[conn_fd].id - 902000;
                        if ((id < 1) || (id > 20)) {
                            valid = false;
                        }
                    }

                    if (!valid) {
                        sprintf(buf, "[Error] Operation failed. Please try again.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        endConnection(conn_fd, &master_set);
                        break;
                    }


#ifdef READ_SERVER
                    if ((lock_reg(file_fd, F_SETLK, F_RDLCK, sizeof(record) * (requestP[conn_fd].id - 1),
                                  SEEK_SET,
                                  sizeof(record)) == -1)) {//有人在write 不能上rdlck
                        sprintf(buf, "Locked.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        endConnection(conn_fd, &master_set);
                        break;
                    } else {
//                        setreadlock(file_fd, requestP[conn_fd].conn_fd);
                        readlock[id]++;
                        lseek(file_fd, sizeof(record) * (id - 1), SEEK_SET);
                        read(file_fd, &temp, sizeof(record));
                        sprintf(buf, "Food: %d booked\n"
                                     "Concert: %d booked\n"
                                     "Electronics: %d booked\n\n"
                                     "(Type Exit to leave...)\n",
                                temp.bookingState[0],
                                temp.bookingState[1],
                                temp.bookingState[2]);
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    }

#elif defined WRITE_SERVER
                    //print record
                    if (!writelock[id] &&
                        lock_reg(file_fd, F_SETLK, F_WRLCK, sizeof(record) * (requestP[conn_fd].id - 1),
                                 SEEK_SET,
                                 sizeof(record)) != -1) {
                        writelock[id] = true;
                        lseek(file_fd, sizeof(record) * (id - 1), SEEK_SET);
                        read(file_fd, &temp, sizeof(record));
                        sprintf(buf,
                                "Food: %d booked\n"
                                "Concert: %d booked\n"
                                "Electronics: %d booked\n\n"
                                "Please input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n",
                                temp.bookingState[0],
                                temp.bookingState[1],
                                temp.bookingState[2]);
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    } else {
                        sprintf(buf, "Locked.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        endConnection(conn_fd, &master_set);
                        break;
                    }
#endif
                    requestP[conn_fd].wait_for_write = true;
                } else {// after enter id
                    int id = requestP[conn_fd].id - 902000;

#ifdef READ_SERVER
                    ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
//                    fprintf(stderr, "ret = %d\n", ret);
                    if (ret <= 0) {
                        readlock[id]--;
                        if (readlock[id] == 0) {
                            unlock(file_fd, requestP[conn_fd].conn_fd);
                        }
                        endConnection(conn_fd, &master_set);
                        break;
                    }
                    if (!strcmp("Exit", requestP[conn_fd].buf)) {//type Exit
                        readlock[id]--;
                        if (readlock[id] == 0) {
                            unlock(file_fd, requestP[conn_fd].conn_fd);
                        }
                        endConnection(conn_fd, &master_set);
                    }

#elif defined WRITE_SERVER
                    ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
//                    fprintf(stderr, "ret = %d\n", ret);
                    if (ret <= 0) {
                        unlock(file_fd, requestP[conn_fd].conn_fd);
                        writelock[id] = false;
                        endConnection(conn_fd, &master_set);
                        break;
                    }
                    // input modification
                    int modi[3];
                    sscanf(requestP[conn_fd].buf, "%d %d %d", &modi[0], &modi[1], &modi[2]);
                    char check[100];
                    sprintf(check, "%d %d %d", modi[0], modi[1], modi[2]);
                    if ((strcmp(check, requestP[conn_fd].buf)) != 0) {
                        sprintf(buf, "[Error] Operation failed. Please try again.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        writelock[id] = false;
                        unlock(file_fd, requestP[conn_fd].conn_fd);
                        endConnection(conn_fd, &master_set);
                    } else {
                        int new[3];
                        bool newrecord = true;
                        int sum = 0;
                        lseek(file_fd, sizeof(record) * (id - 1), SEEK_SET);
                        read(file_fd, &temp, sizeof(record));
                        for (int j = 0; j < 3; j++) {
                            new[j] = temp.bookingState[j] + modi[j];
                            sum += new[j];
                        }

                        //check modification valid
                        if ((sum > 15)) {
                            sprintf(buf, "[Error] Sorry, but you cannot book more than 15 items in total.\n");
                            write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                            newrecord = false;
                            unlock(file_fd, requestP[conn_fd].conn_fd);
                            writelock[id] = false;
                            endConnection(conn_fd, &master_set);
                        } else {
                            for (int j = 0; j < 3; j++) {
                                if (new[j] < 0) {
                                    sprintf(buf, "[Error] Sorry, but you cannot book less than 0 items.\n");
                                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                                    newrecord = false;
                                    unlock(file_fd, requestP[conn_fd].conn_fd);
                                    writelock[id] = false;
                                    endConnection(conn_fd, &master_set);
                                    break;
                                }
                            }
                        }
                        //change booking record
                        if (newrecord) {
                            lseek(file_fd, sizeof(record) * (id - 1), SEEK_SET);
                            read(file_fd, &temp, sizeof(record));
                            for (int j = 0; j < 3; j++) {
                                temp.bookingState[j] = new[j];
                            }
                            //print
                            sprintf(buf,
                                    "Bookings for user %d are updated, the new booking state is:\n"
                                    "Food: %d booked\n"
                                    "Concert: %d booked\n"
                                    "Electronics: %d booked\n",
                                    requestP[conn_fd].id, new[0], new[1], new[2]);
                            write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                            lseek(file_fd, sizeof(record) * (id - 1), SEEK_SET);
                            write(file_fd, &temp, sizeof(record));
                            unlock(file_fd, requestP[conn_fd].conn_fd);
                            writelock[id] = false;
                            endConnection(conn_fd, &master_set);

                        }
                    }
#endif
                }

                break;
            }
        }


    }
    free(requestP);
    return 0;

}
// ======================================================================================================
// You don't need to know how the following codes are working


static void init_request(request *reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->wait_for_write = false;
}

static void free_request(request *reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request *) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}