#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<assert.h>
#include<math.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<errno.h>
#include<sys/mman.h>
#include<pthread.h>

#define MAX_DEPTH 5

// error handling
#define ERR_EXIT(a) do {perror(a);exit(1);} while(0)

// max number of movies
#define MAX_MOVIES 63000

// max length of each movie
#define MAX_LEN 0xff

// number of requests
#define MAX_REQ 0xff

// number of genre
#define NUM_OF_GENRE 19

// number of l1,l2 threads
#define MAX_CPU 0x200
#define MAX_THREAD 0x200

void sort(char **movies, double *pts, int size);

typedef struct request {
    int id;
    char *keywords;
    double *profile;
} request;

typedef struct movie_profile {
    int movieId;
    char *title;
    double *profile;
} movie_profile;

typedef struct filtered {
    int filtered_num;
    char *title[MAX_MOVIES];
    double scores[MAX_MOVIES];
} filtered_movies;

typedef struct pa {
    int depth;
    int left;
    int right;
    filtered_movies *filtered;
} parameter;