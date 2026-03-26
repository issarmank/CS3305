#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define N 9
#define TOTAL_THREADS 27

typedef struct {
    int thread_id;   // 1..27
    int kind;        // 0=subgrid, 1=row, 2=column
    int index;       // 0..8
    int start_row;   // for subgrid
    int start_col;   // for subgrid
} Task;

static int grid[N][N];
static int results[TOTAL_THREADS]; // 1 valid, 0 invalid
static Task tasks[TOTAL_THREADS];

static int check_set_1_to_9(const int vals[N]) {
    int seen[10] = {0}; // index 1..9
    for (int i = 0; i < N; i++) {
        int v = vals[i];
        if (v < 1 || v > 9) return 0;
        if (seen[v]) return 0;
        seen[v] = 1;
    }
    return 1;
}

static void *worker(void *arg) {
    Task *t = (Task *)arg;

    int vals[N];
    int ok = 0;

    if (t->kind == 1) { // row
        for (int c = 0; c < N; c++) vals[c] = grid[t->index][c];
        ok = check_set_1_to_9(vals);
    } else if (t->kind == 2) { // column
        for (int r = 0; r < N; r++) vals[r] = grid[r][t->index];
        ok = check_set_1_to_9(vals);
    } else { // subgrid
        int k = 0;
        for (int r = t->start_row; r < t->start_row + 3; r++) {
            for (int c = t->start_col; c < t->start_col + 3; c++) {
                vals[k++] = grid[r][c];
            }
        }
        ok = check_set_1_to_9(vals);
    }

    results[t->thread_id - 1] = ok;
    return NULL;
}

static void print_thread_result(const Task *t, int ok) {
    if (t->kind == 0) {
        printf("Thread # %d (subgrid %d) is %s \n",
               t->thread_id, t->index + 1, ok ? "valid" : "INVALID");
    } else if (t->kind == 1) {
        printf("Thread # %d (row %d) is %s \n",
               t->thread_id, t->index + 1, ok ? "valid" : "INVALID");
    } else {
        printf("Thread # %d (column %d) is %s \n",
               t->thread_id, t->index + 1, ok ? "valid" : "INVALID");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sudoku-file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (fscanf(fp, "%d", &grid[r][c]) != 1) {
                fprintf(stderr, "Error: could not read 9x9 grid from '%s'\n", filename);
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);

    pthread_t threads[TOTAL_THREADS];

    int tid = 1;

    // 9 subgrid threads
    for (int sg = 0; sg < 9; sg++, tid++) {
        int sr = (sg / 3) * 3;
        int sc = (sg % 3) * 3;

        tasks[tid - 1] = (Task){
            .thread_id = tid,
            .kind = 0,
            .index = sg,
            .start_row = sr,
            .start_col = sc
        };

        if (pthread_create(&threads[tid - 1], NULL, worker, &tasks[tid - 1]) != 0) {
            fprintf(stderr, "Error: pthread_create failed (thread %d)\n", tid);
            return 1;
        }
    }

    // 9 row threads
    for (int r = 0; r < 9; r++, tid++) {
        tasks[tid - 1] = (Task){
            .thread_id = tid,
            .kind = 1,
            .index = r,
            .start_row = 0,
            .start_col = 0
        };

        if (pthread_create(&threads[tid - 1], NULL, worker, &tasks[tid - 1]) != 0) {
            fprintf(stderr, "Error: pthread_create failed (thread %d)\n", tid);
            return 1;
        }
    }

    // 9 column threads
    for (int c = 0; c < 9; c++, tid++) {
        tasks[tid - 1] = (Task){
            .thread_id = tid,
            .kind = 2,
            .index = c,
            .start_row = 0,
            .start_col = 0
        };

        if (pthread_create(&threads[tid - 1], NULL, worker, &tasks[tid - 1]) != 0) {
            fprintf(stderr, "Error: pthread_create failed (thread %d)\n", tid);
            return 1;
        }
    }

    // Parent gathers results (wait for all 27 child threads)
    for (int i = 0; i < TOTAL_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Parent prints in deterministic order 1 to 27
    int all_valid = 1;
    for (int i = 0; i < TOTAL_THREADS; i++) {
        int ok = results[i];
        print_thread_result(&tasks[i], ok);
        if (!ok) all_valid = 0;
    }

    if (all_valid) {
        printf("%s contains a valid solution \n", filename);
    } else {
        printf("%s contains an INVALID solution \n", filename);
    }

    return 0;
}


