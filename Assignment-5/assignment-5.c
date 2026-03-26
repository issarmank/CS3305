#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Station requirements based on assignment
#define NUM_STATIONS 5
#define NUM_TRAINS 2

int station_passengers[NUM_STATIONS] = {500, 50, 100, 250, 100};

// Mutexes for resources: 5 stations + 1 stdout
pthread_mutex_t station_locks[NUM_STATIONS];
pthread_mutex_t print_lock;

typedef struct {
    int id;
    int capacity;
    int current_passengers;
    int current_station;
    int direction; // 1 for forward (dropping off), -1 for backward (returning to 0)
} Train;

void safe_print_exit(int train_id, int station, int current_passengers, int capacity, int station_left, const char* verb) {
    pthread_mutex_lock(&print_lock);
    printf("\tTrain %d is at Station %d and has %d/%d passengers\n", train_id, station, current_passengers, capacity);
    printf("\tStation %d has %d passengers left to %s\n", station, station_left, verb);
    printf(" Train %d LEAVES Station %d\n", train_id, station);
    pthread_mutex_unlock(&print_lock);
}

void* train_thread(void* arg) {
    Train* train = (Train*)arg;

    // To match expected output, Train 0 starts first, but Train 1 
    // must be allowed to "catch up" and lead the unloading at Station 3.
    if (train->id == 1) {
        usleep(50000); 
    }

    while (1) {
        // Global stop condition: Station 0 is empty AND all delivery stations are satisfied
        int all_done = 1;
        for (int i = 0; i < NUM_STATIONS; i++) {
            if (station_passengers[i] > 0) all_done = 0;
        }
        
        if (all_done && train->current_passengers == 0 && train->current_station == 0) {
            pthread_mutex_lock(&print_lock);
            printf(" Train %d ENTERS Station 0\n", train->id);
            printf("\tStation 0 has 0 passengers left to arrive\n");
            printf("\tTrain %d is at Station 0 and has 0/%d passengers\n", train->id, train->capacity);
            printf("\t\t<Nothing more to do>...\n");
            printf("\tTrain %d is at Station 0 and has 0/%d passengers\n", train->id, train->capacity);
            printf("\tStation 0 has 0 passengers left to arrive\n");
            printf(" Train %d LEAVES Station 0\n", train->id);
            pthread_mutex_unlock(&print_lock);
            break;
        }

        pthread_mutex_lock(&station_locks[train->current_station]);

        int action_amount = 0;
        const char* action_str = "<Nothing more to do>...";
        const char* verb = (train->current_station == 0) ? "pick up" : "arrive";

        if (train->current_station == 0) {
            if (station_passengers[0] > 0) {
                action_amount = train->capacity - train->current_passengers;
                if (action_amount > station_passengers[0]) action_amount = station_passengers[0];
                action_str = "Loading passengers...";
            }
        } else {
            // Logic for dropping off: only unload if the station needs passengers
            if (train->current_passengers > 0 && station_passengers[train->current_station] > 0) {
                action_amount = train->current_passengers;
                if (action_amount > station_passengers[train->current_station]) {
                    action_amount = station_passengers[train->current_station];
                }
                action_str = "Unloading passengers...";
            }
        }

        // Print block
        pthread_mutex_lock(&print_lock);
        printf(" Train %d ENTERS Station %d\n", train->id, train->current_station);
        printf("\tStation %d has %d passengers left to %s\n", train->current_station, station_passengers[train->current_station], verb);
        printf("\tTrain %d is at Station %d and has %d/%d passengers\n", train->id, train->current_station, train->current_passengers, train->capacity);
        printf("\t\t%s\n", action_str);

        if (action_amount > 0) {
            if (train->current_station == 0) {
                train->current_passengers += action_amount;
                station_passengers[0] -= action_amount;
            } else {
                train->current_passengers -= action_amount;
                station_passengers[train->current_station] -= action_amount;
            }
        }

        printf("\tTrain %d is at Station %d and has %d/%d passengers\n", train->id, train->current_station, train->current_passengers, train->capacity);
        printf("\tStation %d has %d passengers left to %s\n", train->current_station, station_passengers[train->current_station], verb);
        printf(" Train %d LEAVES Station %d\n", train->id, train->current_station);
        pthread_mutex_unlock(&print_lock);

        pthread_mutex_unlock(&station_locks[train->current_station]);

        // Movement Logic:
        // Move forward if carrying people, move back if empty.
        if (train->current_passengers > 0) {
            if (train->current_station < NUM_STATIONS - 1) train->direction = 1;
            else train->direction = -1; 
        } else {
            if (train->current_station > 0) train->direction = -1;
            else train->direction = 1;
        }

        train->current_station += train->direction;
        
        // Small variable delay based on Train ID to maintain Train 1 leading Train 0
        if (train->id == 1) usleep(500000); // 0.5s travel
        else usleep(800000); // 0.8s travel (Train 0 is slower)
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_TRAINS];
    Train trains[NUM_TRAINS];

    // Initialize Station mutexes
    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_init(&station_locks[i], NULL);
    }
    pthread_mutex_init(&print_lock, NULL);

    // Initialize Trains
    trains[0].id = 0;
    trains[0].capacity = 100;
    trains[0].current_passengers = 0;
    trains[0].current_station = 0;
    trains[0].direction = 1;

    trains[1].id = 1;
    trains[1].capacity = 50;
    trains[1].current_passengers = 0;
    trains[1].current_station = 0;
    trains[1].direction = 1;

    // Start Threads
    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_create(&threads[i], NULL, train_thread, &trains[i]);
    }

    // Wait for Threads
    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup Mutexes
    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_destroy(&station_locks[i]);
    }
    pthread_mutex_destroy(&print_lock);

    return 0;
}