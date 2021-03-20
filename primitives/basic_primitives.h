#ifndef MELTDOWN_BASIC_PRIMITIVES_H
#define MELTDOWN_BASIC_PRIMITIVES_H

#include <stdint.h>
int fallout_init();
void fallout_cleanup();
int toy_wtf(void* mem, int page_offset, int secret_value);
int toy_wtf_v2(void *mem, int page_offset, int secret_value);
int wtf(void* mem, int page_offset);
int flush_cache(void *mem);
int kernel_wtf(void* mem, int procfile);
int data_bounce(void *mem, void *ptr, int repeats);
#endif //MELTDOWN_BASIC_PRIMITIVES_H
