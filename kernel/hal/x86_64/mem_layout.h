#pragma once

#include <stdint.h>

extern uintptr_t kernel_base;
extern uintptr_t kernel_phys_base;

extern void *ke_text_segment_start;
extern void *ke_text_segment_end;
extern void *ke_data_segment_start;
extern void *ke_data_segment_end;
extern void *kernel_end;
