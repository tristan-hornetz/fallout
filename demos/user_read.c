#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../primitives/basic_primitives.h"
#define REPS 20000

int main(int argc, char** args){
    printf("Demo 1: Write Transient Forwarding, version 1.\nNote: This demo is not breaking any access restrictions.\n");
    int page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size,page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    time_t t;
    time(&t);
    srand(t);
    int reps = REPS, success = 0;
    fallout_init();
    printf("Attempting %d random reads...\n", reps);
    char *loading = "-\\|/";
    while (reps--){
        if(reps % 1000 == 0) {
            printf("[%c]   \r", loading[(reps / 1000) % 4]);
            fflush(stdout);
        }
        int offset = rand()%page_size, secret_number = rand()%256;
        success += toy_wtf(mem, offset, secret_number);
    }
    free(mem);
    printf("%d of %d reads succeeded.\nSuccess rate: %.2f %%\n", success, REPS, ((double) success * 100) / REPS);
    fallout_cleanup();
    return 0;
}


