#include "../primitives/basic_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define REPS 500

int main() {
    if (!fallout_compatible()) {
        printf("Your Processor Architecture is not supported by this demo!\n");
        return 1;
    }
    int page_size = getpagesize();
    void* not_mapped = NULL + 1;
    void* mapped = aligned_alloc(page_size, page_size);
    uint8_t *mem = aligned_alloc(page_size, page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    printf("Demo 3: Data bounces\nuser_read1");
    char *loading = "-\\|/";
    int success = 0;
    for(int i = 0; i < REPS; i++){
        if(i % 25 == 0) {printf("[%2.0f%%  %c]                \r", (i*100.0)/REPS, loading[(i / 25) % 4]);fflush(stdout);}
        success += data_bounce(mem, mapped, 100) > data_bounce(mem, not_mapped, 100) * 2;
    }
    free(mem);
    fallout_cleanup();
    printf("%d of %d data bounce tests succeeded.\nSuccess rate: %.2f %%\n", success, REPS, ((double) success * 100) / REPS);
    return 0;
}


