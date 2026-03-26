#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

// Helper function to check if a string is a valid positive integer
int is_valid_int(char *str) {
    if (*str == '\0') return 0;
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

// Helper function to determine if a number is prime
// Returns 1 if prime, 0 if not
int is_prime(int num) {
    if (num <= 1) return 0;
    for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) return 0;
    }
    return 1;
}

// Logic to calculate primes in a specific range (inclusive)
void calculate_range(int start, int end) {
    int count = 0;
    long long sum = 0;

    for (int i = start; i <= end; i++) {
        if (is_prime(i)) {
            count++;
            sum += i;
        }
    }

    printf("pid: %d, ppid %d - Count and sum of prime numbers between %d and %d are %d and %lld\n",
           getpid(), getppid(), start, end, count, sum);
}

int main(int argc, char *argv[]) {
    // 1. INPUT VALIDATION

    // Check if we have at least 3 arguments (plus program name = 4)
    if (argc < 4) {
        printf("Error: Please provide at least 3 parameters.\n");
        printf("Usage: %s <mode> <min> <max>\n", argv[0]);
        return 1;
    }

    // Verify first three parameters are integers (ignore extras)
    if (!is_valid_int(argv[1]) || !is_valid_int(argv[2]) || !is_valid_int(argv[3])) {
        printf("Error: All parameters must be integers.\n");
        return 1;
    }

    int mode = atoi(argv[1]); // 0 = Serial, non-zero = Parallel
    int min = atoi(argv[2]);
    int max = atoi(argv[3]); 

    // Check if max is greater than min
    if (max <= min) {
        printf("Error: Third parameter (max) must be strictly larger than second parameter (min).\n");
        return 1;
    }

    // Print parent PID line (required format)
    printf("Process id: %d\n", getpid());

    // 2. RANGE DIVISION (match sample boundaries: 0-25, 25-50, 50-75, 75-100)
    int range_size = max - min;
    int q = range_size / 4;

    int starts[4] = { min, min + q, min + 2 * q, min + 3 * q };
    int ends[4]   = { min + q, min + 2 * q, min + 3 * q, max };

    // 3. EXECUTION LOGIC
    if (mode == 0) {
        // --- SERIAL MODE ---
        for (int i = 0; i < 4; i++) {
            calculate_range(starts[i], ends[i]);
        }
    } else {
        // --- PARALLEL MODE ---
        pid_t pids[4];

        for (int i = 0; i < 4; i++) {
            pids[i] = fork();

            if (pids[i] < 0) {
                perror("Fork failed");
                return 1;
            } else if (pids[i] == 0) {
                // === CHILD PROCESS ===
                calculate_range(starts[i], ends[i]);
                _exit(0);
            }
        }

        // === PARENT PROCESS ===
        for (int i = 0; i < 4; i++) {
            wait(NULL);
        }
    }

    return 0;
}