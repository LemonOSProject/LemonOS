/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#include <lai/internal-util.h>

// ----------------------------------------------------------------------------
// struct lai_variable.
// ----------------------------------------------------------------------------

/* There are several things to note about handles, references and indices:
 * - Handles and references are distinct types!
 *   For example, handles to methods can be created in constant package data.
 *   Those handles are distinct from references returned by RefOf(MTHD).
 * - References bind to the variable (LOCALx, ARGx or the namespace node),
 *   while indices bind to the object (e.g., the package inside a LOCALx).
 *   This means that, if a package in a LOCALx variable is replaced by another package,
 *   existing indices still refer to the old package, while existing references
 *   will see the new package. */

// Value types: integer, string, buffer, package.
#define LAI_INTEGER            1
#define LAI_STRING             2
#define LAI_BUFFER             3
#define LAI_PACKAGE            4
// Handle type: this is used to represent device (and other) namespace nodes.
#define LAI_HANDLE             5
#define LAI_LAZY_HANDLE        6
// Reference types: obtained from RefOf() or CondRefOf()
#define LAI_ARG_REF            7
#define LAI_LOCAL_REF          8
#define LAI_NODE_REF           9
// Index types: obtained from Index().
#define LAI_STRING_INDEX      10
#define LAI_BUFFER_INDEX      11
#define LAI_PACKAGE_INDEX     12

typedef struct lai_variable_t
{
    int type;
    uint64_t integer;        // for Name()

    union {
        struct lai_string_head *string_ptr;
        struct lai_buffer_head *buffer_ptr;
        struct lai_pkg_head *pkg_ptr;
        struct {
            struct lai_nsnode *unres_ctx_handle;
            const uint8_t *unres_aml;
        };
        struct { // LAI_ARG_REF and LAI_LOCAL_REF.
            struct lai_invocation *iref_invocation;
            int iref_index;
        };
    };

    struct lai_nsnode *handle;

    int index;
} lai_variable_t;

struct lai_string_head {
    lai_rc_t rc;
    char *content;
};

struct lai_buffer_head {
    lai_rc_t rc;
    size_t size;
    uint8_t *content;
};

struct lai_pkg_head {
    lai_rc_t rc;
    unsigned int size;
    struct lai_variable_t *elems;
};

// Allows access to the contents of a string.
__attribute__((always_inline))
inline char *lai_exec_string_access(lai_variable_t *str) {
    LAI_ENSURE(str->type == LAI_STRING);
    return str->string_ptr->content;
}

// Returns the size of a string.
size_t lai_exec_string_length(lai_variable_t *str);

// Returns the size of a buffer.
__attribute__((always_inline))
inline size_t lai_exec_buffer_size(lai_variable_t *buffer) {
    LAI_ENSURE(buffer->type == LAI_BUFFER);
    return buffer->buffer_ptr->size;
}

// Allows access to the contents of a buffer.
__attribute__((always_inline))
inline void *lai_exec_buffer_access(lai_variable_t *buffer) {
    LAI_ENSURE(buffer->type == LAI_BUFFER);
    return buffer->buffer_ptr->content;
}

// Returns the size of a package.
__attribute__((always_inline))
inline size_t lai_exec_pkg_size(lai_variable_t *object) {
    // TODO: Ensure that this is a package.
    return object->pkg_ptr->size;
}

// Helper functions for lai_exec_pkg_load()/lai_exec_pkg_store(), for internal interpreter use.
void lai_exec_pkg_var_load(lai_variable_t *out, struct lai_pkg_head *head, size_t i);
void lai_exec_pkg_var_store(lai_variable_t *in, struct lai_pkg_head *head, size_t i);

// Load/store values from/to packages.
__attribute__((always_inline))
inline void lai_exec_pkg_load(lai_variable_t *out, lai_variable_t *pkg, size_t i) {
    LAI_ENSURE(pkg->type == LAI_PACKAGE);
    return lai_exec_pkg_var_load(out, pkg->pkg_ptr, i);
}
__attribute__((always_inline))
inline void lai_exec_pkg_store(lai_variable_t *in, lai_variable_t *pkg, size_t i) {
    LAI_ENSURE(pkg->type == LAI_PACKAGE);
    return lai_exec_pkg_var_store(in, pkg->pkg_ptr, i);
}

// ----------------------------------------------------------------------------
// struct lai_operand.
// ----------------------------------------------------------------------------

// Name types: unresolved names and names of certain objects.
#define LAI_OPERAND_OBJECT    1
#define LAI_NULL_NAME         2
#define LAI_UNRESOLVED_NAME   3
#define LAI_RESOLVED_NAME     4
#define LAI_ARG_NAME          5
#define LAI_LOCAL_NAME        6
#define LAI_DEBUG_NAME        7

// While struct lai_object can store all AML *objects*, this struct can store all expressions
// that occur as operands in AML. In particular, this includes objects and references to names.
struct lai_operand {
    int tag;
    union {
        lai_variable_t object;
        int index; // Index of ARGx and LOCALx.
        struct {
            struct lai_nsnode *unres_ctx_handle;
            const uint8_t *unres_aml;
        };
        struct lai_nsnode *handle;
    };
};

// ----------------------------------------------------------------------------
// struct lai_state.
// ----------------------------------------------------------------------------

#define LAI_POPULATE_STACKITEM         1
#define LAI_METHOD_STACKITEM           2
#define LAI_LOOP_STACKITEM             3
#define LAI_COND_STACKITEM             4
#define LAI_BUFFER_STACKITEM           5
#define LAI_PACKAGE_STACKITEM          6
#define LAI_NODE_STACKITEM             7 // Parse a namespace leaf node (i.e., not a scope).
#define LAI_OP_STACKITEM               8 // Parse an operator.
#define LAI_INVOKE_STACKITEM           9 // Parse a method invocation.
#define LAI_RETURN_STACKITEM          10 // Parse a return operand
#define LAI_BANKFIELD_STACKITEM       11 // Parse a BankValue and FieldList

struct lai_invocation {
    lai_variable_t arg[7];
    lai_variable_t local[8];

	// Stores a list of all namespace nodes created by this method.
    struct lai_list per_method_list;
};

struct lai_ctxitem {
    struct lai_aml_segment *amls;
    uint8_t *code;
    struct lai_nsnode *handle; // Context handle for relative AML names.
    struct lai_invocation *invocation;
};

// The block stack stores a program counter (PC) and PC limit.
struct lai_blkitem {
    int pc;
    int limit;
};

#define LAI_LOOP_ITERATION 1
#define LAI_COND_BRANCH 1

typedef struct lai_stackitem_ {
    int kind;
    // For stackitem accepting arguments.
    int opstack_frame;
    // Specific to each type of stackitem:
    union {
        struct {
            uint8_t mth_want_result;
        };
        struct {
            int cond_state;
            int cond_has_else;
            int cond_else_pc;
            int cond_else_limit;
        };
        struct {
            int loop_state;
            int loop_pred; // Loop predicate PC.
        };
        struct {
            uint8_t buf_want_result;
        };
        struct {
            int pkg_index;
            uint8_t pkg_want_result;
        };
        struct {
            int op_opcode;
            uint8_t op_arg_modes[8];
            uint8_t op_want_result;
        };
        struct {
            int node_opcode;
            uint8_t node_arg_modes[8];
        };
        struct {
            int ivk_argc;
            uint8_t ivk_want_result;
        };
    };
} lai_stackitem_t;

#define LAI_SMALL_CTXSTACK_SIZE 8
#define LAI_SMALL_BLKSTACK_SIZE 8
#define LAI_SMALL_STACK_SIZE 16
#define LAI_SMALL_OPSTACK_SIZE 16

typedef struct lai_state_t {
    // Base pointers and stack capacities.
    struct lai_ctxitem *ctxstack_base;
    struct lai_blkitem *blkstack_base;
    lai_stackitem_t *stack_base;
    struct lai_operand *opstack_base;
    int ctxstack_capacity;
    int blkstack_capacity;
    int stack_capacity;
    int opstack_capacity;
    // Current stack pointers.
    int ctxstack_ptr; // Stack to track the current context.
    int blkstack_ptr; // Stack to track the current block.
    int stack_ptr;    // Stack to track the current execution state.
    int opstack_ptr;
    struct lai_ctxitem small_ctxstack[LAI_SMALL_CTXSTACK_SIZE];
    struct lai_blkitem small_blkstack[LAI_SMALL_BLKSTACK_SIZE];
    lai_stackitem_t small_stack[LAI_SMALL_STACK_SIZE];
    struct lai_operand small_opstack[LAI_SMALL_OPSTACK_SIZE];
} lai_state_t;
