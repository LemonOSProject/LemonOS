
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

/* ACPI OperationRegion Implementation */
/* OperationRegions allow ACPI's AML to access I/O ports, system memory, system
 * CMOS, PCI config, and other hardware used for I/O with the chipset. */

#include <lai/core.h>
#include "aml_opcodes.h"
#include "libc.h"
#include "opregion.h"

void lai_read_field(lai_variable_t *, lai_nsnode_t *);
void lai_write_field(lai_nsnode_t *, lai_variable_t *);
void lai_read_indexfield(lai_variable_t *, lai_nsnode_t *);
void lai_write_indexfield(lai_nsnode_t *, lai_variable_t *);

void lai_read_opregion(lai_variable_t *destination, lai_nsnode_t *field) {
    if (field->type == LAI_NAMESPACE_FIELD)
        return lai_read_field(destination, field);

    else if (field->type == LAI_NAMESPACE_INDEXFIELD)
        return lai_read_indexfield(destination, field);

    lai_panic("undefined field read: %s", lai_stringify_node_path(field));
}

void lai_write_opregion(lai_nsnode_t *field, lai_variable_t *source) {
    if (field->type == LAI_NAMESPACE_FIELD)
        return lai_write_field(field, source);

    else if (field->type == LAI_NAMESPACE_INDEXFIELD)
        return lai_write_indexfield(field, source);

    lai_panic("undefined field write: %s", lai_stringify_node_path(field));
}

void lai_read_field(lai_variable_t *destination, lai_nsnode_t *field) {
    lai_nsnode_t *opregion = field->fld_region_node;

    uint64_t offset, value, mask;
    size_t bit_offset;

    mask = ((uint64_t)1 << field->fld_size);
    mask--;
    offset = field->fld_offset / 8;
    void *mmio;

    // these are for PCI
    LAI_CLEANUP_VAR lai_variable_t bus_number = LAI_VAR_INITIALIZER;
    LAI_CLEANUP_VAR lai_variable_t seg_number = LAI_VAR_INITIALIZER;
    LAI_CLEANUP_VAR lai_variable_t address_number = LAI_VAR_INITIALIZER;
    uint64_t bbn_result = 0; // When _BBN is not present, we assume PCI bus 0.
    uint64_t seg_result = 0; // When _SEG is not present, we default to Segment Group 0
    uint64_t adr_result = 0; // When _ADR is not present, again, default to zero.
    size_t pci_byte_offset;

    if (opregion->op_address_space != ACPI_OPREGION_PCI) {
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                bit_offset = field->fld_offset % 8;
                break;

            case FIELD_WORD_ACCESS:
                bit_offset = field->fld_offset % 16;
                offset &= (~1);        // clear lowest bit
                break;

            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                bit_offset = field->fld_offset % 32;
                offset &= (~3);        // clear lowest two bits
                break;

            case FIELD_QWORD_ACCESS:
                bit_offset = field->fld_offset % 64;
                offset &= (~7);        // clear lowest three bits
                break;

            default:
                lai_panic("undefined field flags 0x%02x: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else {
        bit_offset = field->fld_offset % 32;
        pci_byte_offset = field->fld_offset % 4;
    }

    if(opregion->op_override){
        switch (field->fld_flags & 0x0F) {
        case FIELD_BYTE_ACCESS:
            if(!opregion->op_override->readb)
                lai_panic("Opregion override doesn't provide readb function");
            value = opregion->op_override->readb(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_WORD_ACCESS:
            if(!opregion->op_override->readw)
                lai_panic("Opregion override doesn't provide readw function");
            value = opregion->op_override->readw(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_DWORD_ACCESS:
        case FIELD_ANY_ACCESS:
            if(!opregion->op_override->readd)
                lai_panic("Opregion override doesn't provide readd function");
            value = opregion->op_override->readd(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_QWORD_ACCESS:
            if(!opregion->op_override->readq)
                lai_panic("Opregion override doesn't provide readq function");
            value = opregion->op_override->readq(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;
        
        default:
            lai_panic("undefined field flags 0x%02x: %s", field->fld_flags,
                          lai_stringify_node_path(field));
            break;
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_IO) {
        // I/O port
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                if (!laihost_inb)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_inb(opregion->op_base + offset) >> bit_offset;
                break;
            case FIELD_WORD_ACCESS:
                if (!laihost_inw)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_inw(opregion->op_base + offset) >> bit_offset;
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                if (!laihost_ind)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_ind(opregion->op_base + offset) >> bit_offset;
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_MEMORY) {
        // Memory-mapped I/O
        if (!laihost_map)
            lai_panic("host does not provide memory mapping functions");
        mmio = laihost_map(opregion->op_base + offset, 8);
        uint8_t *mmio_byte;
        uint16_t *mmio_word;
        uint32_t *mmio_dword;
        uint64_t *mmio_qword;

        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                mmio_byte = (uint8_t*)mmio;
                value = (uint64_t)mmio_byte[0] >> bit_offset;
                break;
            case FIELD_WORD_ACCESS:
                mmio_word = (uint16_t*)mmio;
                value = (uint64_t)mmio_word[0] >> bit_offset;
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                mmio_dword = (uint32_t*)mmio;
                value = (uint64_t)mmio_dword[0] >> bit_offset;
                break;
            case FIELD_QWORD_ACCESS:
                mmio_qword = (uint64_t*)mmio;
                value = mmio_qword[0] >> bit_offset;
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_PCI) {
        LAI_CLEANUP_STATE lai_state_t state;
        lai_init_state(&state);

        // PCI seg number is in the _SEG object.
        lai_nsnode_t *seg_handle = lai_resolve_search(opregion, "_SEG");
        if (seg_handle) {
            if (lai_eval(&seg_number, seg_handle, &state))
                lai_panic("could not evaluate _SEG of OperationRegion()");
            bbn_result = seg_number.integer;
        }

        // PCI bus number is in the _BBN object.
        lai_nsnode_t *bbn_handle = lai_resolve_search(opregion, "_BBN");
        if (bbn_handle) {
            if (lai_eval(&bus_number, bbn_handle, &state))
                lai_panic("could not evaluate _BBN of OperationRegion()");
            bbn_result = bus_number.integer;
        }

        // Device slot/function is in the _ADR object.
        lai_nsnode_t *adr_handle = lai_resolve_search(opregion, "_ADR");
        if (adr_handle) {
            if (lai_eval(&address_number, adr_handle, &state))
                lai_panic("could not evaluate _ADR of OperationRegion()");
            adr_result = address_number.integer;
        }

       switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                if (!laihost_pci_readb) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readb((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFF) + opregion->op_base);
                break;
            case FIELD_WORD_ACCESS:
                if (!laihost_pci_readw) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readw((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFE) + opregion->op_base);
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                if (!laihost_pci_readd) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readd((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFC) + opregion->op_base);
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else {
        lai_panic("undefined opregion address space: %d", opregion->op_address_space);
    }

    destination->type = LAI_INTEGER;
    destination->integer = value & mask;
}

void lai_write_field(lai_nsnode_t *field, lai_variable_t *source) {
    // determine the flags we need in order to write
    lai_nsnode_t *opregion = field->fld_region_node;

    uint64_t offset, value, mask;
    size_t bit_offset;

    mask = ((uint64_t)1 << field->fld_size);
    mask--;
    offset = field->fld_offset / 8;
    void *mmio;

    // these are for PCI
    LAI_CLEANUP_VAR lai_variable_t bus_number = LAI_VAR_INITIALIZER;
    LAI_CLEANUP_VAR lai_variable_t seg_number = LAI_VAR_INITIALIZER;
    LAI_CLEANUP_VAR lai_variable_t address_number = LAI_VAR_INITIALIZER;
    uint64_t bbn_result = 0; // When _BBN is not present, we assume PCI bus 0.
    uint64_t seg_result = 0; // When _SEG is not present, we default to Segment Group 0
    uint64_t adr_result = 0; // When _ADR is not present, again, default to zero.
    size_t pci_byte_offset;

    if (opregion->op_address_space != ACPI_OPREGION_PCI) {
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                bit_offset = field->fld_offset % 8;
                break;

            case FIELD_WORD_ACCESS:
                bit_offset = field->fld_offset % 16;
                offset &= (~1);        // clear lowest bit
                break;

            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                bit_offset = field->fld_offset % 32;
                offset &= (~3);        // clear lowest two bits
                break;

            case FIELD_QWORD_ACCESS:
                bit_offset = field->fld_offset % 64;
                offset &= (~7);        // clear lowest three bits
                break;

            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else {
        bit_offset = field->fld_offset % 32;
        pci_byte_offset = field->fld_offset % 4;
    }

    if(opregion->op_override){
        switch (field->fld_flags & 0x0F) {
        case FIELD_BYTE_ACCESS:
            if(!opregion->op_override->readb)
                lai_panic("Opregion override doesn't provide readb function");
            value = opregion->op_override->readb(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_WORD_ACCESS:
            if(!opregion->op_override->readw)
                lai_panic("Opregion override doesn't provide readw function");
            value = opregion->op_override->readw(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_DWORD_ACCESS:
        case FIELD_ANY_ACCESS:
            if(!opregion->op_override->readd)
                lai_panic("Opregion override doesn't provide readd function");
            value = opregion->op_override->readd(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;

        case FIELD_QWORD_ACCESS:
            if(!opregion->op_override->readq)
                lai_panic("Opregion override doesn't provide readq function");
            value = opregion->op_override->readq(opregion->op_base + offset, opregion->op_userptr) >> bit_offset;
            break;
        
        default:
            lai_panic("undefined field flags 0x%02x: %s", field->fld_flags,
                          lai_stringify_node_path(field));
            break;
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_IO) {
        // I/O port
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                if (!laihost_inb)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_inb(opregion->op_base + offset);
                break;
            case FIELD_WORD_ACCESS:
                if (!laihost_inw)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_inw(opregion->op_base + offset);
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                if (!laihost_ind)
                    lai_panic("host does not provide port I/O functions");
                value = (uint64_t)laihost_ind(opregion->op_base + offset);
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_MEMORY) {
        // Memory-mapped I/O
        if (!laihost_map)
            lai_panic("host does not provide memory mapping functions");
        mmio = laihost_map(opregion->op_base + offset, 8);
        uint8_t *mmio_byte;
        uint16_t *mmio_word;
        uint32_t *mmio_dword;
        uint64_t *mmio_qword;

        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                mmio_byte = (uint8_t *)mmio;
                value = (uint64_t)mmio_byte[0];
                break;
            case FIELD_WORD_ACCESS:
                mmio_word = (uint16_t *)mmio;
                value = (uint64_t)mmio_word[0];
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                mmio_dword = (uint32_t *)mmio;
                value = (uint64_t)mmio_dword[0];
                break;
            case FIELD_QWORD_ACCESS:
                mmio_qword = (uint64_t *)mmio;
                value = mmio_qword[0];
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_PCI) {
        LAI_CLEANUP_STATE lai_state_t state;
        lai_init_state(&state);

        // PCI seg number is in the _SEG object.
        lai_nsnode_t *seg_handle = lai_resolve_search(opregion, "_SEG");
        if (seg_handle) {
            if (lai_eval(&seg_number, seg_handle, &state))
                lai_panic("could not evaluate _SEG of OperationRegion()");
            bbn_result = seg_number.integer;
        }

        // PCI bus number is in the _BBN object.
        lai_nsnode_t *bbn_handle = lai_resolve_search(opregion, "_BBN");
        if (bbn_handle) {
            if (lai_eval(&bus_number, bbn_handle, &state))
                lai_panic("could not evaluate _BBN of OperationRegion()");
            bbn_result = bus_number.integer;
        }

        // Device slot/function is in the _ADR object.
        lai_nsnode_t *adr_handle = lai_resolve_search(opregion, "_ADR");
        if (adr_handle) {
            if (lai_eval(&address_number, adr_handle, &state))
                lai_panic("could not evaluate _ADR of OperationRegion()");
            adr_result = address_number.integer;
        }

       switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                if (!laihost_pci_readb) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readb((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFF) + opregion->op_base);
                break;
            case FIELD_WORD_ACCESS:
                if (!laihost_pci_readw) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readw((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFE) + opregion->op_base);
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                if (!laihost_pci_readd) lai_panic("host does not provide PCI access functions");
                value = laihost_pci_readd((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFC) + opregion->op_base);
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }
    } else {
        lai_panic("undefined opregion address space: %d", opregion->op_address_space);
    }

    // now determine how we need to write to the field
    if (((field->fld_flags >> 5) & 0x0F) == FIELD_PRESERVE) {
        value &= ~(mask << bit_offset);
        value |= (source->integer << bit_offset);
    } else if (((field->fld_flags >> 5) & 0x0F) == FIELD_WRITE_ONES) {
        value = 0xFFFFFFFFFFFFFFFF;
        value &= ~(mask << bit_offset);
        value |= (source->integer << bit_offset);
    } else {
        value = 0;
        value |= (source->integer << bit_offset);
    }

    if(opregion->op_override) {
        switch (field->fld_flags & 0x0F) {
        case FIELD_BYTE_ACCESS:
            if(!opregion->op_override->writeb)
                lai_panic("Opregion override doesn't provide writeb function");
            opregion->op_override->writeb(opregion->op_base + offset, value, opregion->op_userptr);
            break;

        case FIELD_WORD_ACCESS:
            if(!opregion->op_override->writew)
                lai_panic("Opregion override doesn't provide writew function");
            opregion->op_override->writew(opregion->op_base + offset, value, opregion->op_userptr);
            break;

        case FIELD_DWORD_ACCESS:
        case FIELD_ANY_ACCESS:
            if(!opregion->op_override->writed)
                lai_panic("Opregion override doesn't provide writed function");
            opregion->op_override->writed(opregion->op_base + offset, value, opregion->op_userptr);
            break;

        case FIELD_QWORD_ACCESS:
            if(!opregion->op_override->writeq)
                lai_panic("Opregion override doesn't provide writeq function");
            opregion->op_override->writeq(opregion->op_base + offset, value, opregion->op_userptr);
            break;
        
        default:
            lai_panic("undefined field flags 0x%02x: %s", field->fld_flags,
                          lai_stringify_node_path(field));
            break;
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_IO) {
        // I/O port
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                laihost_outb(opregion->op_base + offset, (uint8_t)value);
                break;
            case FIELD_WORD_ACCESS:
                laihost_outw(opregion->op_base + offset, (uint16_t)value);
                //lai_debug("wrote 0x%X to I/O port 0x%X", (uint16_t)value, opregion->op_base + offset);
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                laihost_outd(opregion->op_base + offset, (uint32_t)value);
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }

        // iowait() equivalent
        laihost_outb(0x80, 0x00);
        laihost_outb(0x80, 0x00);
    } else if (opregion->op_address_space == ACPI_OPREGION_MEMORY) {
        // Memory-mapped I/O
        if (!laihost_map)
            lai_panic("host does not provide memory mapping functions");
        mmio = laihost_map(opregion->op_base + offset, 8);
        uint8_t *mmio_byte;
        uint16_t *mmio_word;
        uint32_t *mmio_dword;
        uint64_t *mmio_qword;

        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                mmio_byte = (uint8_t*)mmio;
                mmio_byte[0] = (uint8_t)value;
                break;
            case FIELD_WORD_ACCESS:
                mmio_word = (uint16_t*)mmio;
                mmio_word[0] = (uint16_t)value;
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                mmio_dword = (uint32_t*)mmio;
                mmio_dword[0] = (uint32_t)value;
                break;
            case FIELD_QWORD_ACCESS:
                mmio_qword = (uint64_t*)mmio;
                mmio_qword[0] = value;
                //lai_debug("wrote 0x%lX to MMIO address 0x%lX", value, opregion->op_base + offset);
                break;
            default:
                lai_panic("undefined field flags 0x%02X", field->fld_flags);
        }
    } else if (opregion->op_address_space == ACPI_OPREGION_PCI) {
        switch (field->fld_flags & 0x0F) {
            case FIELD_BYTE_ACCESS:
                if (!laihost_pci_writeb) lai_panic("host does not provide PCI access functions");
                laihost_pci_writeb((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFF) + opregion->op_base,
                                 (uint8_t)value);
                break;
            case FIELD_WORD_ACCESS:
                if (!laihost_pci_writew) lai_panic("host does not provide PCI access functions");
                laihost_pci_writew((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFE) + opregion->op_base,
                                 (uint16_t)value);
                break;
            case FIELD_DWORD_ACCESS:
            case FIELD_ANY_ACCESS:
                if (!laihost_pci_writed) lai_panic("host does not provide PCI access functions");
                laihost_pci_writed((uint16_t)seg_result,
                                 (uint8_t)bbn_result,
                                 (uint8_t)(adr_result >> 16) & 0xFF,
                                 (uint8_t)(adr_result & 0xFF),
                                 (offset & 0xFFFC) + opregion->op_base,
                                 (uint32_t)value);
                break;
            default:
                lai_panic("undefined field flags 0x%02X: %s", field->fld_flags,
                          lai_stringify_node_path(field));
        }



    } else {
        lai_panic("undefined opregion address space: %d", opregion->op_address_space);
    }
}

void lai_read_indexfield(lai_variable_t *dest, lai_nsnode_t *idxf) {
    lai_nsnode_t *index_field = idxf->idxf_index_node;
    lai_nsnode_t *data_field = idxf->idxf_data_node;

    lai_variable_t index = {0};
    index.type = LAI_INTEGER;
    index.integer = idxf->idxf_offset / 8; // Always byte-aligned.

    lai_write_field(index_field, &index); // Write index register.
    lai_read_field(dest, data_field); // Read data register.
}

void lai_write_indexfield(lai_nsnode_t *idxf, lai_variable_t *src) {
    lai_nsnode_t *index_field = idxf->idxf_index_node;
    lai_nsnode_t *data_field = idxf->idxf_data_node;

    lai_variable_t index = {0};
    index.type = LAI_INTEGER;
    index.integer = idxf->idxf_offset / 8; // Always byte-aligned.

    lai_write_field(index_field, &index); // Write index register.
    lai_write_field(data_field, src); // Write data register.
}