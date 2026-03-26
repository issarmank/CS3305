#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Helper functionto send two integers and receive one product back
int get_product_from_child(int p2c[2], int c2p[2], int val1, int val2, pid_t p_pid) {
    int res;
    printf("Parent (PID %d): Sending %d to child \n", p_pid, val1);
    write(p2c[1], &val1, sizeof(int));
    printf("Parent (PID %d): Sending %d to child \n", p_pid, val2);
    write(p2c[1], &val2, sizeof(int));
    
    read(c2p[0], &res, sizeof(int));
    printf("Parent (PID %d): Received %d from child \n", p_pid, res);
    return res;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <4-digit-int1> <4-digit-int2>\n", argv[0]);
        return 1;
    }

    int num_a = atoi(argv[1]);
    int num_b = atoi(argv[2]);

    // Range Validation
    if (num_a < 1000 || num_a > 9999 || num_b < 1000 || num_b > 9999) {
        printf("Error: Please provide integers between 1000 and 9999.\n");
        return 1;
    }

    printf("Your integers are %d %d\n", num_a, num_b);

    int a1 = num_a / 100, a2 = num_a % 100;
    int b1 = num_b / 100, b2 = num_b % 100;

    int p2c[2], c2p[2];

    // Create both pipes before forking
    if (pipe(p2c) == -1 || pipe(c2p) == -1) {
        perror("Pipe failed");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid > 0) {
        close(p2c[0]); 
        close(c2p[1]);
        pid_t p_pid = getpid();
        printf("Parent (PID %d): created child (PID %d) \n\n", p_pid, pid);

        // Calculating X
        printf("### \n# Calculating X \n### \n");
        int A = get_product_from_child(p2c, c2p, a1, b1, p_pid);
        long X = (long)A * 10000;

        // Calculating Y
        printf("\n### \n# Calculating Y \n### \n");
        int C = get_product_from_child(p2c, c2p, a1, b2, p_pid);
        int B = get_product_from_child(p2c, c2p, a2, b1, p_pid);
        long Y = (long)(B + C) * 100;

        // Calculating Z 
        printf("\n### \n# Calculating Z \n### \n");
        int D = get_product_from_child(p2c, c2p, a2, b2, p_pid);
        long Z = D;

        long final_sum = X + Y + Z;
        printf("\n%d*%d == %ld + %ld + %ld == %ld\n", num_a, num_b, X, Y, Z, final_sum);

        close(p2c[1]);
        close(c2p[0]);
        wait(NULL); // Clean up child
    } 
    else {
        close(p2c[1]);
        close(c2p[0]);
        pid_t c_pid = getpid();

        int v1, v2, prod;
        // The child expects 4 pairs of numbers (A, B, C, D)
        for (int i = 0; i < 4; i++) {
            if (read(p2c[0], &v1, sizeof(int)) > 0 && read(p2c[0], &v2, sizeof(int)) > 0) {
                printf("Child (PID %d): Received %d from parent \n", c_pid, v1);
                printf("Child (PID %d): Received %d from parent \n", c_pid, v2);
                prod = v1 * v2;
                printf("Child (PID %d): Sending %d to parent \n", c_pid, prod);
                write(c2p[1], &prod, sizeof(int));
            }
        }
        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    }

    return 0;
}