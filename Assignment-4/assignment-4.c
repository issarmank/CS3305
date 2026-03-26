#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define MAX_PROCESSES 1000

// Retains all tracked data per process as requested
typedef struct {
    int id;
    int arrival_time;
    int initial_burst;
    int burst_left;
    int wait_time;
    int turnaround_time;
    bool completed;
} Process;

// Simple array-based circular queue for Round Robin
int queue[MAX_PROCESSES * 2];
int head = 0;
int tail = 0;

void enqueue(int process_id) {
    queue[tail++] = process_id;
}

int dequeue() {
    if (head < tail) return queue[head++];
    return -1;
}

// FCFS Selection Logic
int get_next_fcfs(int current_time, Process* p, int n) {
    for (int i = 0; i < n; i++) {
        if (p[i].arrival_time <= current_time && !p[i].completed) {
            return i;
        }
    }
    return -1;
}

// Preemptive SJF Selection Logic
int get_next_sjf(int current_time, Process* p, int n, int current_active) {
    int best_idx = -1;
    int min_burst = 999999;
    
    // Pass 1: find the absolute minimum remaining burst
    for (int i = 0; i < n; i++) {
        if (p[i].arrival_time <= current_time && !p[i].completed) {
            if (p[i].burst_left < min_burst) {
                min_burst = p[i].burst_left;
            }
        }
    }
    
    // Pass 2: Tie-breaker - "Tie goes to the currently running process"
    if (current_active != -1 && !p[current_active].completed && p[current_active].burst_left == min_burst) {
        return current_active;
    }
    
    // Pass 3: Fallback Tie-breaker - Lowest Process ID
    for (int i = 0; i < n; i++) {
        if (p[i].arrival_time <= current_time && !p[i].completed && p[i].burst_left == min_burst) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    int algo = 0; // 1: FCFS, 2: SJF, 3: RR
    int quantum = 0;
    char *filename = NULL;

    // --- Command Line Argument Parsing ---
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [-f | -s | -r <quantum>] <filename>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-f") == 0) {
        algo = 1;
        filename = argv[2];
    } else if (strcmp(argv[1], "-s") == 0) {
        algo = 2;
        filename = argv[2];
    } else if (strcmp(argv[1], "-r") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: -r requires a quantum and a filename.\n");
            return 1;
        }
        algo = 3;
        quantum = atoi(argv[2]);
        filename = argv[3];
    } else {
        return 1;
    }

    // --- File Reading ---
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file.\n");
        return 1;
    }

    Process processes[MAX_PROCESSES];
    int num_processes = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        char *token = strtok(line, ",\n\r");
        while (token != NULL) {
            int val = atoi(token);
            processes[num_processes].id = num_processes;
            processes[num_processes].arrival_time = num_processes;
            processes[num_processes].initial_burst = val;
            processes[num_processes].burst_left = val;
            processes[num_processes].wait_time = 0;
            processes[num_processes].turnaround_time = 0;
            processes[num_processes].completed = false;
            num_processes++;
            token = strtok(NULL, ",\n\r");
        }
    }
    fclose(fp);

    // --- Output Headers ---
    if (algo == 1) printf("First Come First Served \n");
    if (algo == 2) printf("Shortest Job First\n");
    if (algo == 3) printf("Round Robin with Quantum %d\n", quantum);

    // --- Core Simulation Loop ---
    int current_time = 0;
    int completed_count = 0;
    int active = -1;
    int time_in_quantum = 0;

    while (completed_count < num_processes) {
        
        // RR Rule 1: Newly arrived processes go to the back of the queue FIRST
        if (algo == 3) {
            for (int i = 0; i < num_processes; i++) {
                if (processes[i].arrival_time == current_time) {
                    enqueue(i);
                }
            }
        }

        // RR Rule 2: Preempt the active process if its quantum has expired
        if (algo == 3 && active != -1 && !processes[active].completed && time_in_quantum == quantum) {
            enqueue(active); 
            active = -1;
        }

        // Determine the active process for this tick
        if (active == -1 || processes[active].completed) {
            if (algo == 1) active = get_next_fcfs(current_time, processes, num_processes);
            else if (algo == 2) active = get_next_sjf(current_time, processes, num_processes, active);
            else if (algo == 3) {
                active = dequeue();
                time_in_quantum = 0; // Reset quantum for the new process
            }
        } else if (algo == 2) {
            // SJF is preemptive; must evaluate every tick
            active = get_next_sjf(current_time, processes, num_processes, active);
        }

        // Print the tick state (spaced exactly like the assignment examples)
        if (active != -1) {
            printf("T%-3d: P%-2d - Burst left %2d, Wait time %3d, Turnaround time %3d\n",
                   current_time, active, processes[active].burst_left,
                   processes[active].wait_time, processes[active].turnaround_time);
        }

        // Apply processing time to active
        if (active != -1) {
            processes[active].burst_left--;
            processes[active].turnaround_time++;
            time_in_quantum++;
            
            if (processes[active].burst_left == 0) {
                processes[active].completed = true;
                completed_count++;
            }
        }

        // Update Wait and Turnaround for all OTHER waiting processes
        for (int i = 0; i < num_processes; i++) {
            if (i != active && processes[i].arrival_time <= current_time && !processes[i].completed) {
                processes[i].wait_time++;
                processes[i].turnaround_time++;
            }
        }
        
        current_time++;
    }

    // --- Final Summary Block ---
    double total_wait = 0.0, total_ta = 0.0;
    for (int i = 0; i < num_processes; i++) {
        printf("\nP%d\n", i);
        printf("        Waiting time:      %5d\n", processes[i].wait_time);
        printf("        Turnaround time:   %5d\n", processes[i].turnaround_time);
        
        total_wait += processes[i].wait_time;
        total_ta += processes[i].turnaround_time;
    }

    double avg_wait = total_wait / num_processes;
    double avg_ta = total_ta / num_processes;
    
    // Truncating down to 1 decimal place to perfectly match the prompt's math
    printf("\nTotal average waiting time:     %.1f\n", floor(avg_wait * 10) / 10.0);
    printf("Total average turnaround time:  %.1f\n", floor(avg_ta * 10) / 10.0);

    return 0;
}