
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

/* System Control Interrupt Initialization */

#include <lai/helpers/sci.h>
#include "../core/libc.h"
#include "../core/exec_impl.h"

// read contents of event registers.
uint16_t lai_get_sci_event(void) {
    if (!laihost_inw || !laihost_outw)
        lai_panic("lai_read_event() requires port I/O");

    struct lai_instance *instance = lai_current_instance();

    uint16_t a = 0, b = 0;
    if (instance->fadt->pm1a_event_block) {
        a = laihost_inw(instance->fadt->pm1a_event_block);
        laihost_outw(instance->fadt->pm1a_event_block, a);
    }
    if (instance->fadt->pm1b_event_block) {
        b = laihost_inw(instance->fadt->pm1b_event_block);
        laihost_outw(instance->fadt->pm1b_event_block, b);
    }
    return a | b;
}

// set event enable registers
void lai_set_sci_event(uint16_t value) {
    if (!laihost_inw || !laihost_outw)
        lai_panic("lai_set_event() requires port I/O");

    struct lai_instance *instance = lai_current_instance();

    uint16_t a = instance->fadt->pm1a_event_block + (instance->fadt->pm1_event_length / 2);
    uint16_t b = instance->fadt->pm1b_event_block + (instance->fadt->pm1_event_length / 2);

    if (instance->fadt->pm1a_event_block)
        laihost_outw(a, value);

    if (instance->fadt->pm1b_event_block)
        laihost_outw(b, value);

    lai_debug("wrote event register value 0x%04X", value);
}

// lai_enable_acpi(): Enables ACPI SCI
// Param:   uint32_t mode - IRQ mode (ACPI spec section 5.8.1)
// Return:  int - 0 on success

int lai_enable_acpi(uint32_t mode) {
    lai_nsnode_t *handle;
    lai_state_t state;
    lai_debug("attempt to enable ACPI...");

    struct lai_instance *instance = lai_current_instance();

    if (!laihost_inw || !laihost_outb)
        lai_panic("lai_enable_acpi() requires port I/O");
    if (!laihost_sleep)
        lai_panic("host does not provide timer functions required by lai_enable_acpi()");

    /* first run \._SB_._INI */
    handle = lai_resolve_path(NULL, "\\_SB_._INI");
    if (handle) {
        lai_init_state(&state);
        if (!lai_eval(NULL, handle, &state))
            lai_debug("evaluated \\_SB_._INI");
        lai_finalize_state(&state);
    }

    /* _STA/_INI for all devices */
    handle = lai_resolve_path(NULL, "\\_SB_");
    lai_init_children(handle);

    /* tell the firmware about the IRQ mode */
    handle = lai_resolve_path(NULL, "\\_PIC");
    if (handle) {
        lai_init_state(&state);

        LAI_CLEANUP_VAR lai_variable_t mode_object = LAI_VAR_INITIALIZER;
        mode_object.type = LAI_INTEGER;
        mode_object.integer = mode;

        if (!lai_eval_largs(NULL, handle, &state, &mode_object, NULL))
            lai_debug("evaluated \\._PIC(%d)", mode);
        lai_finalize_state(&state);
    }

    /* enable ACPI SCI */
    laihost_outb(instance->fadt->smi_command_port, instance->fadt->acpi_enable);
    laihost_sleep(10);

    for (size_t i = 0; i < 100; i++) {
        if (laihost_inw(instance->fadt->pm1a_control_block) & ACPI_ENABLED)
            break;

        laihost_sleep(10);
    }

    /* set FADT event fields */
    lai_set_sci_event(ACPI_POWER_BUTTON | ACPI_SLEEP_BUTTON | ACPI_WAKE);
    lai_get_sci_event();

    lai_debug("ACPI is now enabled.");
    return 0;
}

int lai_disable_acpi(void){
    if (!laihost_inw || !laihost_outw)
        lai_panic("lai_read_event() requires port I/O");

    struct lai_instance *instance = lai_current_instance();

    lai_debug("attempt to disable ACPI...");

    // Disable all SCI events
    lai_set_sci_event(0);
    lai_get_sci_event();

    // Clear SCI_EN (APCI_ENABLED in lai) so to stop SCIs from arriving
    uint16_t pm1a_cnt_block = laihost_inw(instance->fadt->pm1a_control_block);
    pm1a_cnt_block &= ~ACPI_ENABLED;
    laihost_outw(instance->fadt->pm1a_control_block, pm1a_cnt_block);

    if(instance->fadt->pm1b_control_block){
        uint16_t pm1b_cnt_block = laihost_inw(instance->fadt->pm1b_control_block);
        pm1b_cnt_block &= ~ACPI_ENABLED;
        laihost_outw(instance->fadt->pm1b_control_block, pm1b_cnt_block);
    }

    // Send the definitive ACPI_DISABLE command
    laihost_outb(instance->fadt->smi_command_port, instance->fadt->acpi_disable);

    lai_debug("Success");
}

int lai_evaluate_sta(lai_nsnode_t *node) {
    // If _STA not present, assume 0x0F as ACPI spec says.
    uint64_t sta = 0x0F;

    lai_nsnode_t *handle = lai_resolve_path(node, "_STA");
    if (handle) {
        LAI_CLEANUP_STATE lai_state_t state;
        lai_init_state(&state);

        LAI_CLEANUP_VAR lai_variable_t result = LAI_VAR_INITIALIZER;
        if (lai_eval(&result, handle, &state))
            lai_panic("could not evaluate _STA");
        if(lai_obj_get_integer(&result, &sta))
            lai_panic("_STA returned non-integer object");
    }

    return sta;
}

void lai_init_children(lai_nsnode_t *parent) {
    lai_nsnode_t *node;
    lai_nsnode_t *handle;

    struct lai_ns_child_iterator iter = LAI_NS_CHILD_ITERATOR_INITIALIZER(parent);

    while ((node = lai_ns_child_iterate(&iter))) {
        if (lai_ns_get_node_type(node) == LAI_NODETYPE_DEVICE) {
            int sta = lai_evaluate_sta(node);

            /* if device is present, evaluate its _INI */
            if (sta & ACPI_STA_PRESENT) {
                handle = lai_resolve_path(node, "_INI");

                if (handle) {
                    lai_state_t state;
                    lai_init_state(&state);
                    if (!lai_eval(NULL, handle, &state)) {
                        LAI_CLEANUP_FREE_STRING char *fullpath = lai_stringify_node_path(handle);
                        lai_debug("evaluated %s", fullpath);
                    }
                    lai_finalize_state(&state);
                }
            }

            /* if functional and/or present, enumerate the children */
            if (sta & ACPI_STA_PRESENT || sta & ACPI_STA_FUNCTION)
                lai_init_children(node);
        }
    }
}
