// #define PROCESS
#include "header.h"

#include <stdbool.h>
movie_profile *movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;
unsigned int num_of_threads = 0;

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request *reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tids[MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

void *create_shared_memory(size_t size) {
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size, protection, visibility, -1, 0);
}

void merge(parameter *left, parameter *right, filtered_movies *filtered) {
//    printf("left->left = %d, left->right = %d, right->left = %d, right->right = %d\n", left->left, left->right,
//           right->left, right->right);
    filtered_movies *mergedL = (filtered_movies *) malloc(sizeof(filtered_movies));
    filtered_movies *mergedR = (filtered_movies *) malloc(sizeof(filtered_movies));

//    int idx = 0;
    int i, j, k;
    int n1 = left->right - left->left + 1;
    int n2 = right->right - right->left + 1;
    for (i = 0; i < n1; i++) {
        mergedL->title[i] = filtered->title[left->left + i];
        mergedL->scores[i] = filtered->scores[left->left + i];
//        printf("%f\n",filtered->scores[left->left + i]);
//        printf("mergedL | score[%d] = %f, title = %s\n", i + left->left, mergedL->scores[i], mergedL->title[i]);
    }
    for (j = 0; j < n2; j++) {
        mergedR->title[j] = filtered->title[right->left + j];
        mergedR->scores[j] = filtered->scores[right->left + j];
//        printf("%f\n",filtered->scores[left->left + j]);
//        printf("mergedR | score[%d] = %f, title = %s\n", j + right->left, mergedR->scores[j], mergedR->title[j]);

    }
    i = 0, j = 0, k = left->left;
    while (i < n1 && j < n2) {
        if (mergedL->scores[i] > mergedR->scores[j]) {
            filtered->title[k] = mergedL->title[i];
            filtered->scores[k] = mergedL->scores[i];
            i++;
        } else if (mergedL->scores[i] < mergedR->scores[j]) {
            filtered->title[k] = mergedR->title[j];
            filtered->scores[k] = mergedR->scores[j];
            j++;
        } else {
            if (strcmp(mergedL->title[i], mergedR->title[j]) < 0) {// s1<s2
                filtered->title[k] = mergedL->title[i];
                filtered->scores[k] = mergedL->scores[i];
                i++;
            } else {
                filtered->title[k] = mergedR->title[j];
                filtered->scores[k] = mergedR->scores[j];
                j++;
            }
        }
        k++;
    }
    while (i < n1) {
        filtered->title[k] = mergedL->title[i];
        filtered->scores[k] = mergedL->scores[i];
        i++;
        k++;
    }
    while (j < n2) {
        filtered->title[k] = mergedR->title[j];
        filtered->scores[k] = mergedR->scores[j];
        j++;
        k++;
    }
//    free(mergedL);
//    free(mergedR);
}

parameter *addparameter(int left, int right, int depth, filtered_movies *filtered) {
    parameter *temp = (parameter *) malloc(sizeof(parameter));
    temp->left = left;
    temp->right = right;
    temp->depth = depth;
    temp->filtered = filtered;
    return temp;
}

filtered_movies *filter(request *key) {
//TODO filter function
//開新陣列紀錄flter後的movie 資訊

#ifdef PROCESS
    filtered_movies *moviefilter = create_shared_memory(sizeof(filtered_movies));
    for(int i=0;i<MAX_MOVIES;i++) {
        moviefilter->title[i] = create_shared_memory(sizeof(char) * MAX_LEN);
    }
#elif defined THREAD

    filtered_movies *moviefilter = (filtered_movies *) malloc(sizeof(filtered_movies));
#endif
//    for (int i = 0; i < MAX_MOVIES; i++) {
//        moviefilter->title[i] = (char *) malloc(sizeof(char) * MAX_LEN);
//    }

    moviefilter->filtered_num = 0;
    char *search = key->keywords;
    if (strcmp(search, "*") == 0) {
        for (int i = 0; i < num_of_movies; i++) {


#ifdef PROCESS
            memcpy(moviefilter->title[moviefilter->filtered_num], movies[i]->title, MAX_LEN);
#elif defined THREAD
            moviefilter->title[moviefilter->filtered_num] = movies[i]->title;
#endif

            for (int j = 0; j < NUM_OF_GENRE; j++) {
                moviefilter->scores[moviefilter->filtered_num] += key->profile[j] * movies[i]->profile[j];
            }
            moviefilter->filtered_num++;
        }
    } else {
        char *title;
        for (int i = 0; i < num_of_movies; i++) {
            title = movies[i]->title;
//            printf("dbg1 %d\n", i);
            if (strstr(title, search) != NULL) {
#ifdef PROCESS
                memcpy(moviefilter->title[moviefilter->filtered_num], movies[i]->title, MAX_LEN);
#elif defined THREAD
                moviefilter->title[moviefilter->filtered_num] = movies[i]->title;
#endif
                for (int j = 0; j < NUM_OF_GENRE; j++) {
//                    printf("genre %d\n", j);
                    moviefilter->scores[moviefilter->filtered_num] += key->profile[j] * movies[i]->profile[j];
                }
                moviefilter->filtered_num++;
            }
        }
    }
//    printf("success filter\n");
    return moviefilter;
}

void thread_merge_sort(parameter *key) {
    //把參數取出來
    int left = key->left;
    int right = key->right;
    int depth = key->depth;
//    printf("left = %d, right = %d, depth = %d\n", left, right, depth);
    filtered_movies *filtered = key->filtered;

    //如果她在最下層 直接sort
    if (depth >= MAX_DEPTH || right == left) {
        sort(filtered->title + left, filtered->scores + left, right - left + 1);
    }
        //如果不在最下層 拆開mergesort再merge
    else {
        int mid = (right + left) / 2;
        parameter *leftmerge = addparameter(left, mid, depth + 1, filtered);
        parameter *rightmerge = addparameter(mid + 1, right, depth + 1, filtered);
        if (num_of_threads < MAX_THREAD) {
            //1. 如果thread# < thread max開thread
            //分成main thread & create thread 分別執行左右的mergesort
            // thread #++
            pthread_t tid1;
            num_of_threads++;
            pthread_create(&tid1, NULL, (void *) thread_merge_sort, (void *) leftmerge);
            thread_merge_sort(rightmerge);
            //只要有開thread 就要join
            pthread_join(tid1, NULL);
            num_of_threads--;
        } else {    // 2. thread max
            // main thread 執行左右mergersort
            //last merge
//            leftmerge->depth++;
//            rightmerge->depth++;
            thread_merge_sort(leftmerge);
            thread_merge_sort(rightmerge);
        }
        merge(leftmerge, rightmerge, filtered);
    }
}

void process_merge_sort(parameter *key) {
    int left = key->left;
    int right = key->right;
    int depth = key->depth;
    int length = right - left + 1;
//    printf("left = %d, right = %d, depth = %d\n", left, right, depth);
    filtered_movies *filtered = key->filtered;

    //如果她在最下層 直接sort
    if (depth >= MAX_DEPTH || right - left < 1024) {
//        printf("dbg\n");
        filtered_movies *temp = (filtered_movies *) malloc(sizeof(filtered_movies));
        for (int i = 0; i < length; i++) {
            temp->title[i] = (char *) malloc(sizeof(char) * MAX_LEN);
            memcpy(temp->title[i], filtered->title[left + i], MAX_LEN);

//            temp->title[i] = filtered->title[left+i];
            temp->scores[i] = filtered->scores[left + i];
        }
//        printf("before sort\n");
        /*if (temp->title + left == NULL)
            printf("eight moon sugar is good!\n");*/
        sort(temp->title , temp->scores , length);
//        printf("after sort\n");
        for (int i = 0; i < length; i++) {
//            printf("%d %s\n",i, temp->title[i]);
            memcpy(filtered->title[left + i], temp->title[i], MAX_LEN);
//            printf("%d %s\n",i, filtered->title[left + i]);

            filtered->scores[left + i] = temp->scores[i];
        }

    }
        //如果不在最下層 拆開mergesort再merge
    else {
        int mid = (right + left) / 2;
        parameter *leftmerge = addparameter(left, mid, depth + 1, filtered);
        parameter *rightmerge = addparameter(mid + 1, right, depth + 1, filtered);
        pid_t pid1 = fork();
        if (pid1 == 0) // child
        {
            process_merge_sort(leftmerge);
            exit(0);
        } else {
            process_merge_sort(rightmerge);
//            wait(NULL);
            waitpid(pid1, NULL, 0);
            merge(leftmerge, rightmerge, filtered);
        }
    }
}

void handle_request(request *key) {
    // TODO handle request function
//裡面有filter function
//    printf("handle_request start!\n");
    filtered_movies *afterfilter = filter(key);
    parameter *first = addparameter(0, afterfilter->filtered_num - 1, 0, afterfilter);
    thread_merge_sort(first);
    FILE *fp;
    char filename[MAX_LEN];
    sprintf(filename, "%dt.out", key->id);
    fp = fopen(filename, "w");
    for (int i = 0; i < afterfilter->filtered_num; i++) {
//        printf("i = %d, %f | %s\n", i, afterfilter->scores[i], afterfilter->title[i]);
        fprintf(fp, "%s\n", afterfilter->title[i]);
    }
    fclose(fp);
    free(afterfilter);
    free(first);

}

void initialize(FILE *fp);

request *read_request();

int main(int argc, char *argv[]) {

    if (argc != 1) {
#ifdef PROCESS
        fprintf(stderr,"usage: ./pserver\n");
        // TODO write the sorted search result to {id}p.out file.
#elif defined THREAD
        fprintf(stderr, "usage: ./tserver\n");
        // TODO write the sorted search result to {id}t.out file.
#endif
        exit(-1);
    }

    FILE *fp;

    if ((fp = fopen("./data/movies.txt", "r")) == NULL) {
        ERR_EXIT("fopen");
    }

    initialize(fp);
    assert(fp != NULL);
    fclose(fp);

#ifdef PROCESS

    filtered_movies *afterfilter = filter(reqs[0]);
    parameter *first = addparameter(0, afterfilter->filtered_num - 1, 0, afterfilter);
    process_merge_sort(first);
    FILE *output;
    char filename[MAX_LEN];
    sprintf(filename, "%dp.out", reqs[0]->id);
    output = fopen(filename, "w");
    for (int i = 0; i < afterfilter->filtered_num; i++) {
//        printf("i = %d, %f | %s\n", i, afterfilter->scores[i], afterfilter->title[i]);
        fprintf(output, "%s\n", afterfilter->title[i]);
    }
    fclose(output);
    free(first);
    munmap(afterfilter, sizeof(filtered_movies));





#elif defined THREAD
    for (int i = 0; i < num_of_reqs; i++) {
        num_of_threads++;
        pthread_create(&tids[i], NULL, (void *) handle_request, reqs[i]);
    }

    for (int i = 0; i < num_of_reqs; i++) {
        pthread_join(tids[i], NULL);
        num_of_threads--;
    }
#endif
    //for loop 開thread (# req)
    // create (handlerequest )

    return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request *read_request() {
    int id;
    char buf1[MAX_LEN], buf2[MAX_LEN];
    char delim[2] = ",";

    char *keywords;
    char *token, *ref_pts;
    char *ptr;
    double ret, sum;

    scanf("%u %254s %254s", &id, buf1, buf2);
    keywords = malloc(sizeof(char) * strlen(buf1) + 1);
    if (keywords == NULL) {
        ERR_EXIT("malloc");
    }

    memcpy(keywords, buf1, strlen(buf1));
    keywords[strlen(buf1)] = '\0';

    double *profile = malloc(sizeof(double) * NUM_OF_GENRE);
    if (profile == NULL) {
        ERR_EXIT("malloc");
    }
    sum = 0;
    ref_pts = strtok(buf2, delim);
    for (int i = 0; i < NUM_OF_GENRE; i++) {
        ret = strtod(ref_pts, &ptr);
        profile[i] = ret;
        sum += ret * ret;
        ref_pts = strtok(NULL, delim);
    }

    // normalize
    sum = sqrt(sum);
    for (int i = 0; i < NUM_OF_GENRE; i++) {
        if (sum == 0)
            profile[i] = 0;
        else
            profile[i] /= sum;
    }

    request *r = malloc(sizeof(request));
    if (r == NULL) {
        ERR_EXIT("malloc");
    }

    r->id = id;
    r->keywords = keywords;
    r->profile = profile;

    return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE *fp) {

    char chunk[MAX_LEN] = {0};
    char *token, *ptr;
    double ret, sum;
    int cnt = 0;

    assert(fp != NULL);

    // first row
    if (fgets(chunk, sizeof(chunk), fp) == NULL) {
        ERR_EXIT("fgets");
    }

    memset(movies, 0, sizeof(movie_profile *) * MAX_MOVIES);

    while (fgets(chunk, sizeof(chunk), fp) != NULL) {

        assert(cnt < MAX_MOVIES);
        chunk[MAX_LEN - 1] = '\0';

        const char delim1[2] = " ";
        const char delim2[2] = "{";
        const char delim3[2] = ",";
        unsigned int movieId;
        movieId = atoi(strtok(chunk, delim1));

        // title
        token = strtok(NULL, delim2);
        char *title = malloc(sizeof(char) * strlen(token) + 1);
        if (title == NULL) {
            ERR_EXIT("malloc");
        }

        // title.strip()
        memcpy(title, token, strlen(token) - 1);
        title[strlen(token) - 1] = '\0';

        // genres
        double *profile = malloc(sizeof(double) * NUM_OF_GENRE);
        if (profile == NULL) {
            ERR_EXIT("malloc");
        }

        sum = 0;
        token = strtok(NULL, delim3);
        for (int i = 0; i < NUM_OF_GENRE; i++) {
            ret = strtod(token, &ptr);
            profile[i] = ret;
            sum += ret * ret;
            token = strtok(NULL, delim3);
        }

        // normalize
        sum = sqrt(sum);
        for (int i = 0; i < NUM_OF_GENRE; i++) {
            if (sum == 0)
                profile[i] = 0;
            else
                profile[i] /= sum;
        }

        movie_profile *m = malloc(sizeof(movie_profile));
        if (m == NULL) {
            ERR_EXIT("malloc");
        }

        m->movieId = movieId;
        m->title = title;
        m->profile = profile;

        movies[cnt++] = m;
    }
    num_of_movies = cnt;

    // request
    scanf("%d", &num_of_reqs);
    assert(num_of_reqs <= MAX_REQ);
    for (int i = 0; i < num_of_reqs; i++) {
        reqs[i] = read_request();
    }
}
/*========================================================*/
