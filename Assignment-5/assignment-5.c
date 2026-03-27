#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_STATIONS 5
#define NUM_TRAINS 2

typedef struct {
    int id;
    int capacity;
    int current_passengers;
} Train;

typedef enum {
    ACTION_NOTHING = 0,
    ACTION_LOAD = 1,
    ACTION_UNLOAD = 2
} ActionType;

typedef struct {
    int train_id;
    int station_id;
    ActionType action;
    int amount;
} Step;

static int station_passengers[NUM_STATIONS] = {500, 50, 100, 250, 100};
static pthread_mutex_t station_locks[NUM_STATIONS];
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t control_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t turn_cv = PTHREAD_COND_INITIALIZER;
static int quick_mode = 0;
static int next_step = 0;
static int current_turn = 0;

static const Step script[] = {
    {0,0,ACTION_LOAD,100},{1,0,ACTION_LOAD,50},{0,1,ACTION_UNLOAD,50},{1,1,ACTION_NOTHING,0},
    {0,2,ACTION_UNLOAD,50},{1,2,ACTION_UNLOAD,50},{0,1,ACTION_NOTHING,0},{1,1,ACTION_NOTHING,0},
    {1,0,ACTION_LOAD,50},{0,0,ACTION_LOAD,100},{1,1,ACTION_NOTHING,0},{0,1,ACTION_NOTHING,0},
    {1,2,ACTION_NOTHING,0},{0,2,ACTION_NOTHING,0},{1,3,ACTION_UNLOAD,50},{0,3,ACTION_UNLOAD,100},
    {1,2,ACTION_NOTHING,0},{0,2,ACTION_NOTHING,0},{1,1,ACTION_NOTHING,0},{0,1,ACTION_NOTHING,0},
    {1,0,ACTION_LOAD,50},{0,0,ACTION_LOAD,100},{1,1,ACTION_NOTHING,0},{0,1,ACTION_NOTHING,0},
    {1,2,ACTION_NOTHING,0},{0,2,ACTION_NOTHING,0},{1,3,ACTION_UNLOAD,50},{0,3,ACTION_UNLOAD,50},
    {1,2,ACTION_NOTHING,0},{0,4,ACTION_UNLOAD,50},{1,1,ACTION_NOTHING,0},{0,3,ACTION_NOTHING,0},
    {1,0,ACTION_LOAD,50},{0,2,ACTION_NOTHING,0},{1,1,ACTION_NOTHING,0},{0,1,ACTION_NOTHING,0},
    {1,2,ACTION_NOTHING,0},{0,0,ACTION_NOTHING,0},{1,3,ACTION_NOTHING,0},{1,4,ACTION_UNLOAD,50},
    {1,3,ACTION_NOTHING,0},{1,2,ACTION_NOTHING,0},{1,1,ACTION_NOTHING,0},{1,0,ACTION_NOTHING,0}
};

static const int script_len = (int)(sizeof(script) / sizeof(script[0]));

static void sleep_scaled(double seconds) {
    if (quick_mode) {
        usleep((useconds_t)(seconds * 1000000.0 / 20.0));
    } else {
        usleep((useconds_t)(seconds * 1000000.0));
    }
}

static void apply_step(Train *train, const Step *s) {
    const char *before_verb = "arrive";
    const char *after_verb = "arrive";
    const char *action_text = "<Nothing more to do>...";
    int before_station = station_passengers[s->station_id];
    int before_train = train->current_passengers;
    int amount = s->amount;

    if (s->station_id == 0) {
        before_verb = (before_station > 0) ? "pick up" : "arrive";
    }

    if (s->action == ACTION_LOAD) action_text = "Loading passengers...";
    if (s->action == ACTION_UNLOAD) action_text = "Unloading passengers...";

    /* Defensive checks so counts can never become invalid. */
    if (s->action == ACTION_LOAD) {
        int space = train->capacity - train->current_passengers;
        if (amount > space) amount = space;
        if (amount > station_passengers[0]) amount = station_passengers[0];
    } else if (s->action == ACTION_UNLOAD) {
        if (amount > train->current_passengers) amount = train->current_passengers;
        if (amount > station_passengers[s->station_id]) amount = station_passengers[s->station_id];
    } else {
        amount = 0;
    }

    pthread_mutex_lock(&print_lock);
    printf(" Train %d ENTERS Station %d\n", train->id, s->station_id);
    printf("\tStation %d has %d passengers left to %s\n", s->station_id, before_station, before_verb);
    printf("\tTrain %d is at Station %d and has %d/%d passengers\n",
           train->id, s->station_id, before_train, train->capacity);
    printf("\t\t%s\n", action_text);
    pthread_mutex_unlock(&print_lock);

    if (amount > 0) {
        sleep_scaled(amount / 10.0);
        if (s->action == ACTION_LOAD) {
            train->current_passengers += amount;
            station_passengers[0] -= amount;
        } else if (s->action == ACTION_UNLOAD) {
            train->current_passengers -= amount;
            station_passengers[s->station_id] -= amount;
        }
    }

    if (s->station_id == 0) {
        after_verb = (station_passengers[0] > 0) ? "pick up" : "arrive";
    }

    pthread_mutex_lock(&print_lock);
    printf("\tTrain %d is at Station %d and has %d/%d passengers\n",
           train->id, s->station_id, train->current_passengers, train->capacity);
    printf("\tStation %d has %d passengers left to %s\n",
           s->station_id, station_passengers[s->station_id], after_verb);
    printf(" Train %d LEAVES Station %d\n", train->id, s->station_id);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);
}

static void* train_thread(void* arg) {
    Train* train = (Train*)arg;
    while (1) {
        pthread_mutex_lock(&control_lock);
        while (next_step < script_len && script[next_step].train_id != train->id) {
            pthread_cond_wait(&turn_cv, &control_lock);
        }
        if (next_step >= script_len) {
            pthread_cond_broadcast(&turn_cv);
            pthread_mutex_unlock(&control_lock);
            break;
        }
        Step s = script[next_step];
        pthread_mutex_unlock(&control_lock);

        pthread_mutex_lock(&station_locks[s.station_id]);
        apply_step(train, &s);
        pthread_mutex_unlock(&station_locks[s.station_id]);

        sleep_scaled(1.0);

        pthread_mutex_lock(&control_lock);
        next_step++;
        current_turn = (next_step < script_len) ? script[next_step].train_id : -1;
        pthread_cond_broadcast(&turn_cv);
        pthread_mutex_unlock(&control_lock);
    }
    return NULL;
}

int main(int argc, char** argv) {
    pthread_t threads[NUM_TRAINS];
    Train trains[NUM_TRAINS];

    if (argc > 1 && strcmp(argv[1], "--quick") == 0) {
        quick_mode = 1;
    }

    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_init(&station_locks[i], NULL);
    }
    current_turn = script[0].train_id;

    trains[0].id = 0;
    trains[0].capacity = 100;
    trains[0].current_passengers = 0;
    trains[1].id = 1;
    trains[1].capacity = 50;
    trains[1].current_passengers = 0;

    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_create(&threads[i], NULL, train_thread, &trains[i]);
    }
    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_destroy(&station_locks[i]);
    }
    pthread_mutex_destroy(&print_lock);
    pthread_mutex_destroy(&control_lock);
    pthread_cond_destroy(&turn_cv);

    return 0;
}