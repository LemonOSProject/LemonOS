/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <acpispec/resources.h>
#include <acpispec/hw.h>
#include <acpispec/tables.h>
#include <lai/host.h>
#include <lai/internal-exec.h>
#include <lai/internal-ns.h>
#include <lai/internal-util.h>

#define ACPI_MAX_RESOURCES          512

typedef enum lai_api_error {
    LAI_ERROR_NONE,
    LAI_ERROR_OUT_OF_MEMORY,
    LAI_ERROR_TYPE_MISMATCH,
    LAI_ERROR_NO_SUCH_NODE,
    LAI_ERROR_OUT_OF_BOUNDS,
    LAI_ERROR_EXECUTION_FAILURE,
    LAI_ERROR_ILLEGAL_ARGUMENTS,

    /* Evaluating external inputs (e.g., nodes of the ACPI namespace) returned an unexpected result.
     * Unlike LAI_ERROR_EXECUTION_FAILURE, this error does not indicate that
     * execution of AML failed; instead, the resulting object fails to satisfy some
     * expectation (e.g., it is of the wrong type, has an unexpected size, or consists of
     * unexpected contents) */
    LAI_ERROR_UNEXPECTED_RESULT,
    // Error given when end of iterator is reached, nothing to worry about
    LAI_ERROR_END_REACHED,
    LAI_ERROR_UNSUPPORTED,
} lai_api_error_t;

// Convert a lai_api_error_t to a human readable string
const char *lai_api_error_to_string(lai_api_error_t);

struct lai_instance {
    lai_nsnode_t *root_node;

    lai_nsnode_t **ns_array;
    size_t ns_size;
    size_t ns_capacity;

    int acpi_revision;

    acpi_fadt_t *fadt;
};

struct lai_instance *lai_current_instance();

void lai_init_state(lai_state_t *);
void lai_finalize_state(lai_state_t *);

#define LAI_CLEANUP_STATE __attribute__((cleanup(lai_finalize_state)))

struct lai_ns_iterator {
    size_t i;
};

struct lai_ns_child_iterator {
    size_t i;
    lai_nsnode_t *parent;
};

#define LAI_NS_ITERATOR_INITIALIZER {0}
#define LAI_NS_CHILD_ITERATOR_INITIALIZER(x) {0, x}

extern volatile uint16_t lai_last_event;

// The remaining of these functions are OS independent!
// ACPI namespace functions
lai_nsnode_t *lai_create_root(void);
void lai_create_namespace(void);
char *lai_stringify_node_path(lai_nsnode_t *);
lai_nsnode_t *lai_resolve_path(lai_nsnode_t *, const char *);
lai_nsnode_t *lai_resolve_search(lai_nsnode_t *, const char *);
lai_nsnode_t *lai_get_device(size_t);
int lai_check_device_pnp_id(lai_nsnode_t *, lai_variable_t *, lai_state_t *);
lai_nsnode_t *lai_enum(char *, size_t);
void lai_eisaid(lai_variable_t *, char *);
lai_nsnode_t *lai_ns_iterate(struct lai_ns_iterator *);
lai_nsnode_t *lai_ns_child_iterate(struct lai_ns_child_iterator *);

// Namespace functions.

lai_nsnode_t *lai_ns_get_root();
lai_nsnode_t *lai_ns_get_parent(lai_nsnode_t *node);
lai_nsnode_t *lai_ns_get_child(lai_nsnode_t *parent, const char *name);
lai_api_error_t lai_ns_override_opregion(lai_nsnode_t *node, const struct lai_opregion_override *override, void *userptr);
enum lai_node_type lai_ns_get_node_type(lai_nsnode_t *node);

uint8_t lai_ns_get_opregion_address_space(lai_nsnode_t *node);

// Access and manipulation of lai_variable_t.

enum lai_object_type {
    LAI_TYPE_NONE,
    LAI_TYPE_INTEGER,
    LAI_TYPE_STRING,
    LAI_TYPE_BUFFER,
    LAI_TYPE_PACKAGE,
    LAI_TYPE_DEVICE,
};

enum lai_object_type lai_obj_get_type(lai_variable_t *object);
lai_api_error_t lai_obj_get_integer(lai_variable_t *, uint64_t *);
lai_api_error_t lai_obj_get_pkg(lai_variable_t *, size_t, lai_variable_t *);
lai_api_error_t lai_obj_get_handle(lai_variable_t *, lai_nsnode_t **);

lai_api_error_t lai_obj_resize_string(lai_variable_t *, size_t);
lai_api_error_t lai_obj_resize_buffer(lai_variable_t *, size_t);
lai_api_error_t lai_obj_resize_pkg(lai_variable_t *, size_t);

lai_api_error_t lai_obj_to_buffer(lai_variable_t *, lai_variable_t *);
lai_api_error_t lai_obj_to_string(lai_variable_t *, lai_variable_t *, size_t);
lai_api_error_t lai_obj_to_decimal_string(lai_variable_t *, lai_variable_t *);
lai_api_error_t lai_obj_to_hex_string(lai_variable_t *, lai_variable_t *);
lai_api_error_t lai_obj_to_integer(lai_variable_t *, lai_variable_t *);
void lai_obj_clone(lai_variable_t *, lai_variable_t *);

#define LAI_CLEANUP_VAR __attribute__((cleanup(lai_var_finalize)))
#define LAI_VAR_INITIALIZER {0}

void lai_var_finalize(lai_variable_t *);
void lai_var_move(lai_variable_t *, lai_variable_t *);
void lai_var_assign(lai_variable_t *, lai_variable_t *);

// Evaluation of namespace nodes (including control methods).

lai_api_error_t lai_eval_args(lai_variable_t *, lai_nsnode_t *, lai_state_t *,
                              int, lai_variable_t *);
lai_api_error_t lai_eval_largs(lai_variable_t *, lai_nsnode_t *, lai_state_t *, ...);
lai_api_error_t lai_eval_vargs(lai_variable_t *, lai_nsnode_t *, lai_state_t *, va_list);
lai_api_error_t lai_eval(lai_variable_t *, lai_nsnode_t *, lai_state_t *);

// ACPI Control Methods
lai_api_error_t lai_populate(lai_nsnode_t *, struct lai_aml_segment *, lai_state_t *);

// LAI initialization functions
void lai_set_acpi_revision(int);

// LAI debugging functions.

// Trace all opcodes. This will produce *very* verbose output.
void lai_enable_tracing(int enable);
