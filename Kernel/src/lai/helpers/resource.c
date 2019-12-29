
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

/* ACPI Resource Template Implementation */
/* Allows discovering of each device's used resources, and thus is needed
 * for basic system enumeration as well as PCI IRQ routing. */

#include <lai/helpers/resource.h>
#include "../core/libc.h"

#define ACPI_SMALL_IRQ             0x04
#define ACPI_SMALL_DMA             0x05
#define ACPI_SMALL_IO              0x08
#define ACPI_SMALL_FIXED_IO        0x09
#define ACPI_SMALL_FIXED_DMA       0x0A
#define ACPI_SMALL_VENDOR          0x0E
#define ACPI_SMALL_END             0x0F

#define ACPI_LARGE_MEM24           0x81
#define ACPI_LARGE_REGISTER        0x82
#define ACPI_LARGE_MEM32           0x85
#define ACPI_LARGE_FIXED_MEM32     0x86
#define ACPI_LARGE_IRQ             0x89

// read a device's resource info
size_t lai_read_resource(lai_nsnode_t *device, acpi_resource_t *dest) {
    LAI_CLEANUP_STATE lai_state_t state;
    lai_init_state(&state);

	lai_nsnode_t *crs_handle = lai_resolve_path(device, "_CRS");
	if (!crs_handle)
	    return 0;

    LAI_CLEANUP_VAR lai_variable_t buffer = LAI_VAR_INITIALIZER;
    int status = lai_eval(&buffer, crs_handle, &state);
    if (status)
        return 0;

    // read the resource buffer
    size_t count = 0;
    uint8_t *data = lai_exec_buffer_access(&buffer);
    size_t data_size;

    acpi_small_irq_t *small_irq;
    uint16_t small_irq_mask;

    acpi_large_irq_t *large_irq;

    size_t i;

    while (data[0] != 0x79) {
        if (!(data[0] & 0x80)) {
            // small resource descriptor
            data_size = (size_t)data[0] & 7;

            switch (data[0] >> 3) {
                case ACPI_SMALL_END:
                    return count;

                case ACPI_SMALL_IRQ:
                    small_irq = (acpi_small_irq_t*)&data[0];
                    small_irq_mask = small_irq->irq_mask;

                    i = 0;

                    while (small_irq_mask) {
                        if (small_irq_mask & (1 << i)) {
                            dest[count].type = ACPI_RESOURCE_IRQ;
                            dest[count].base = (uint64_t)i;

                            /* ACPI spec says when irq flags are not present, we should
                            assume active high, edge-triggered, exclusive */
                            if (data_size >= 3)
                                dest[count].irq_flags = small_irq->config;
                            else
                                dest[count].irq_flags = ACPI_IRQ_ACTIVE_HIGH
                                    | ACPI_IRQ_EDGE
                                    | ACPI_IRQ_EXCLUSIVE;

                            small_irq_mask &= ~(1 << i);
                            count++;
                        }

                        i++;
                    }

                    data += data_size + 1;
                    break;

                default:
                    lai_debug("acpi warning: undefined small resource, byte 0 is %02X, ignoring...",
                            data[0]);
                    return 0;
            }
        } else {
            // large resource descriptor
            data_size = (size_t)data[1] & 0xFF;
            data_size |= (size_t)((data[2] & 0xFF) << 8);

            switch (data[0]) {
                case ACPI_LARGE_IRQ:
                    large_irq = (acpi_large_irq_t*)&data[0];

                    dest[count].type = ACPI_RESOURCE_IRQ;
                    dest[count].base = (uint64_t)large_irq->irq;
                    dest[count].irq_flags = large_irq->config;

                    count++;
                    data += data_size + 3;
                    break;

                default:
                    lai_debug("acpi warning: undefined large resource, byte 0 is %02X, ignoring...",
                            data[0]);
                    return 0;
            }
        }
    }

    return count;
}

struct lai_resource_header_info {
    uint8_t type;
    size_t size;
    size_t skip_size;
};

static struct lai_resource_header_info lai_get_header_info(uint8_t* header_byte){
    struct lai_resource_header_info info;
    if(!(header_byte[0] & (1 << 7))){ // Small
        info.type = (header_byte[0] >> 3) & 0xF;
        info.size = (header_byte[0] & 0x7);
        info.skip_size = info.size + 1;    
    } else { // Large
        info.type = header_byte[0];
        info.size = header_byte[1] | (header_byte[2] << 8);
        info.skip_size = info.size + 3;
    }
    return info;
}

lai_api_error_t lai_resource_iterate(struct lai_resource_view *iterator){
    LAI_ENSURE(iterator);
    LAI_ENSURE(iterator->entry);

    iterator->entry += iterator->skip_size;

    struct lai_resource_header_info info = lai_get_header_info(iterator->entry);
    uint8_t *entry = iterator->entry;

    switch (info.type){
        case ACPI_SMALL_FIXED_IO:
            iterator->base = (entry[1] | ((entry[2] & 0x3) << 8));
            iterator->length = entry[3];
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
        case ACPI_SMALL_IO: // TODO: Actually support min and max
            iterator->flags = entry[1];
            iterator->base = (entry[2] | (entry[3] << 8));
            iterator->alignment = entry[6];
            iterator->length = entry[7];
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
        case ACPI_SMALL_IRQ:
            iterator->entry_idx = 0;
            if(info.size == 2){
                // No IRQ flags, use defaults
                iterator->flags = ACPI_IRQ_ACTIVE_HIGH | ACPI_IRQ_EDGE | ACPI_IRQ_EXCLUSIVE;
            } else if(info.size == 3){
                iterator->flags = entry[3];
            } else {
                lai_warn("Unknown small IRQ resource size: %02X", info.size);
                return LAI_ERROR_UNEXPECTED_RESULT;
            }
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
        case ACPI_SMALL_END:
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_END_REACHED;
        case ACPI_LARGE_IRQ:
            iterator->entry_idx = 0;
            iterator->flags = entry[3];
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
        case ACPI_LARGE_FIXED_MEM32:
            iterator->flags = entry[3];
            iterator->base = (entry[4] | (entry[5] << 8) | (entry[6] << 16) | (entry[7] << 24));
            iterator->length = (entry[8] | (entry[9] << 8) | (entry[10] << 16) | (entry[11] << 24));
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
        default:
            lai_debug("undefined resource, type is %02X, ignoring...", info.type);
            iterator->skip_size = info.skip_size;
            return LAI_ERROR_NONE;
    }

    return LAI_ERROR_UNEXPECTED_RESULT;    
}

enum lai_resource_type lai_resource_get_type(struct lai_resource_view *iterator){
    LAI_ENSURE(iterator);
    LAI_ENSURE(iterator->entry);
    uint8_t *entry = iterator->entry;

    struct lai_resource_header_info info = lai_get_header_info(entry);
    switch (info.type){
        case ACPI_SMALL_IRQ:
            return LAI_RESOURCE_IRQ;
        case ACPI_SMALL_DMA:
            return LAI_RESOURCE_DMA;
        case ACPI_SMALL_IO:
            return LAI_RESOURCE_IO;
        case ACPI_SMALL_FIXED_IO:
            return LAI_RESOURCE_IO;
        case ACPI_SMALL_FIXED_DMA:
            return LAI_RESOURCE_DMA;
        case ACPI_SMALL_VENDOR:
            return LAI_RESOURCE_VENDOR;
        case ACPI_SMALL_END:
            return LAI_RESOURCE_NULL;
        case ACPI_LARGE_MEM24:
            return LAI_RESOURCE_MEM;
        case ACPI_LARGE_REGISTER:
            return LAI_RESOURCE_REGISTER;
        case ACPI_LARGE_MEM32:
            return LAI_RESOURCE_MEM;
        case ACPI_LARGE_FIXED_MEM32:
            return LAI_RESOURCE_MEM;
        case ACPI_LARGE_IRQ:
            return LAI_RESOURCE_IRQ;
        default:
            lai_debug("Unknown resource type %02X in lai_resource_get_type", info.type);
            return LAI_RESOURCE_NULL;
    }    
}

lai_api_error_t lai_resource_next_irq(struct lai_resource_view *iterator){
    LAI_ENSURE(iterator);
    LAI_ENSURE(iterator->entry);
    uint8_t *entry = iterator->entry;

    struct lai_resource_header_info info = lai_get_header_info(entry);
    

    if(info.type == ACPI_SMALL_IRQ) {
        uint8_t irq_mask = (entry[1] | (entry[2] << 8));
        while(iterator->entry_idx <= 15){
            if(irq_mask & (1 << iterator->entry_idx)){ // It seems this entry is set
                iterator->gsi = iterator->entry_idx;
                iterator->entry_idx++;
                return LAI_ERROR_NONE;
            } else {
                iterator->entry_idx++;
            }
        }
        return LAI_ERROR_END_REACHED;
    } else if(info.type == ACPI_LARGE_IRQ) {
        uint8_t interrupt_table_length = entry[4];
        if(iterator->entry_idx < interrupt_table_length){
            // Still a valid entry
            uint8_t offset = 4 * iterator->entry_idx + 5;
            iterator->gsi = (entry[offset] | (entry[offset + 1] << 8) | (entry[offset + 2] << 16) | (entry[offset + 3] << 24));
            iterator->entry_idx++;
            return LAI_ERROR_NONE;
        }
        return LAI_ERROR_END_REACHED;
    } else { // This resource is not an IRQ
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }
}
