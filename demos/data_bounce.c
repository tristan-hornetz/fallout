#include "../primitives/basic_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define REPS 500

int main() {
    int page_size = getpagesize();
    void *not_mapped = NULL + 1;
    void *mapped = aligned_alloc(page_size, page_size);
    uint8_t *mem = aligned_alloc(page_size, page_size * 256);
    memset(mem, 0xFF, page_size * 256);
    fallout_init();
    printf("Demo 3: Data bounce\n");
    char *loading = "-\\|/";
    int true_positives = 0, false_positives = 0;
    for (int i = 0; i < REPS; i++) {
        if (i % 25 == 0) {
            printf("[%2.0f%%  %c]                \r", (i * 100.0) / REPS, loading[(i / 25) % 4]);
            fflush(stdout);
        }
        true_positives += data_bounce(mem, mapped, 200);
        false_positives += data_bounce(mem, not_mapped, 200);
    }
    free(mem);
    fallout_cleanup();
    printf("%d tests were performed.\n"\
    "    * Rate of hits at a mapped address (true positives): %2.2f%%\n"\
    "    * Rate of hits at an unmapped address (false positives): %2.2f%%\n",
           REPS, ((double) true_positives * 100) / REPS, ((double) false_positives * 100) / REPS);
    return 0;
}


