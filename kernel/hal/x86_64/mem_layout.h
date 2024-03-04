#pragma once

#include <stdint.h>

extern uintptr_t kernel_base;
extern uintptr_t kernel_phys_base;

extern uintptr_t direct_mapping_base;

extern void *ke_text_segment_start;
extern void *ke_text_segment_end;
extern void *ke_data_segment_start;
extern void *ke_data_segment_end;
extern void *kernel_end;

#define ADDRESS_4GB (0x100000000ull)
#define GET_VMEM_MAPPING(x) ((uintptr_t)(x) + direct_mapping_base)
