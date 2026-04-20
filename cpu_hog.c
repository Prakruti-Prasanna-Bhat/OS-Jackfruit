#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Starting CPU hog...\n");

    volatile unsigned long long i = 0;

    while (1) {
        i++;
        if (i % 1000000000 == 0) {
            printf("Still running...\n");
        }
    }

    return 0;
}
