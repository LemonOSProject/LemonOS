
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

// Internal header file. Do not use outside of LAI.

#pragma once

#include <lai/core.h>

struct lai_amlname {
    int is_absolute;   // Is the path absolute or not?
    int height;        // Number of scopes to exit before resolving the name.
                       // In other words, this is the number of ^ in front of the name.
    int search_scopes; // Is the name searched in the scopes of all parents?

    // Internal variables used by the parser.
    const uint8_t *it;
    const uint8_t *end;
};

// Initializes the AML name parser.
// Use lai_amlname_done() + lai_amlname_iterate() to process the name.
size_t lai_amlname_parse(struct lai_amlname *amln, const void *data);

// Returns true if there are no more segments.
int lai_amlname_done(struct lai_amlname *amln);

// Copies the next segment of the name to out.
// out must be a char array of size >= 4.
void lai_amlname_iterate(struct lai_amlname *amln, char *out);

// Turns the AML name into a ASL-like name string.
// Returns a pointer allocated by laihost_malloc().
char *lai_stringify_amlname(const struct lai_amlname *amln);

// This will replace lai_resolve().
lai_nsnode_t *lai_do_resolve(lai_nsnode_t *ctx_handle, const struct lai_amlname *amln);

// Used in the implementation of lai_resolve_new_node().
void lai_do_resolve_new_node(lai_nsnode_t *node,
        lai_nsnode_t *ctx_handle, const struct lai_amlname *amln);

// Evaluate constant data (and keep result).
//     Primitive objects are parsed.
//     Names are left unresolved.
//     Operations (e.g. Add()) are not allowed.
#define LAI_DATA_MODE 1
// Evaluate dynamic data (and keep result).
//     Primitive objects are parsed.
//     Names are resolved. Methods are executed.
//     Operations are allowed and executed.
#define LAI_OBJECT_MODE 2
// Like LAI_OBJECT_MODE, but discard the result.
#define LAI_EXEC_MODE 3
#define LAI_UNRESOLVED_MODE 4
#define LAI_REFERENCE_MODE 5
#define LAI_OPTIONAL_REFERENCE_MODE 6
#define LAI_IMMEDIATE_BYTE_MODE 7
#define LAI_IMMEDIATE_WORD_MODE 8
#define LAI_IMMEDIATE_DWORD_MODE 9

// Operation is expected to return a result (on the opstack).
#define LAI_MF_RESULT   1
// Resolve names to namespace nodes.
#define LAI_MF_RESOLVE  2
// Allow unresolvable names.
#define LAI_MF_NULLABLE 4
// Parse method invocations.
// Requires LAI_MF_RESOLVE.
#define LAI_MF_INVOKE   8

static const uint32_t lai_mode_flags[] = {
    [LAI_IMMEDIATE_BYTE_MODE]     = LAI_MF_RESULT,
    [LAI_IMMEDIATE_WORD_MODE]     = LAI_MF_RESULT,
    [LAI_IMMEDIATE_DWORD_MODE]    = LAI_MF_RESULT,
    [LAI_EXEC_MODE]               = LAI_MF_RESOLVE | LAI_MF_INVOKE,
    [LAI_UNRESOLVED_MODE]         = LAI_MF_RESULT,
    [LAI_DATA_MODE]               = LAI_MF_RESULT,
    [LAI_OBJECT_MODE]             = LAI_MF_RESULT | LAI_MF_RESOLVE | LAI_MF_INVOKE,
    [LAI_REFERENCE_MODE]          = LAI_MF_RESULT | LAI_MF_RESOLVE,
    [LAI_OPTIONAL_REFERENCE_MODE] = LAI_MF_RESULT | LAI_MF_RESOLVE | LAI_MF_NULLABLE,
};

// Allocate a new package.
int lai_create_string(lai_variable_t *, size_t);
int lai_create_c_string(lai_variable_t *, const char *);
int lai_create_buffer(lai_variable_t *, size_t);
int lai_create_pkg(lai_variable_t *, size_t);

void lai_exec_ref_load(lai_variable_t *, lai_variable_t *);
void lai_exec_ref_store(lai_variable_t *, lai_variable_t *);

void lai_exec_access(lai_variable_t *, lai_nsnode_t *);
void lai_store_ns(lai_nsnode_t *target, lai_variable_t *object);
void lai_mutate_ns(lai_nsnode_t *target, lai_variable_t *object);

void lai_operand_load(lai_state_t *, struct lai_operand *, lai_variable_t *);
void lai_operand_mutate(lai_state_t *, struct lai_operand *, lai_variable_t *);
void lai_operand_emplace(lai_state_t *, struct lai_operand *, lai_variable_t *);

void lai_exec_get_objectref(lai_state_t *, struct lai_operand *, lai_variable_t *);
void lai_exec_get_integer(lai_state_t *, struct lai_operand *, lai_variable_t *);

// --------------------------------------------------------------------------------------
// Inline function for context stack manipulation.
// --------------------------------------------------------------------------------------

static inline int lai_exec_reserve_ctxstack(lai_state_t *state) {
    if (state->ctxstack_ptr + 1 == state->ctxstack_capacity) {
        size_t new_capacity = 2 * state->ctxstack_capacity;
        struct lai_ctxitem *new_stack = laihost_malloc(new_capacity * sizeof(struct lai_ctxitem));
        if (!new_stack) {
            lai_warn("failed to allocate memory for context stack");
            return 1;
        }
        memcpy(new_stack, state->ctxstack_base,
               (state->ctxstack_ptr + 1) * sizeof(struct lai_ctxitem));
        if (state->ctxstack_base != state->small_ctxstack)
            laihost_free(state->ctxstack_base);
        state->ctxstack_base = new_stack;
        state->ctxstack_capacity = new_capacity;
    }
    return 0;
}

// Pushes a new item to the context stack and returns it.
static inline struct lai_ctxitem *lai_exec_push_ctxstack(lai_state_t *state) {
    state->ctxstack_ptr++;
    // Users are expected to call the reserve() function before this one.
    LAI_ENSURE(state->ctxstack_ptr < state->ctxstack_capacity);
    memset(&state->ctxstack_base[state->ctxstack_ptr], 0, sizeof(struct lai_ctxitem));
    return &state->ctxstack_base[state->ctxstack_ptr];
}

// Returns the last item of the context stack.
static inline struct lai_ctxitem *lai_exec_peek_ctxstack_back(lai_state_t *state) {
    if (state->ctxstack_ptr < 0)
        return NULL;
    return &state->ctxstack_base[state->ctxstack_ptr];
}

// Removes an item from the context stack.
static inline void lai_exec_pop_ctxstack_back(lai_state_t *state) {
    LAI_ENSURE(state->ctxstack_ptr >= 0);
    struct lai_ctxitem *ctxitem = &state->ctxstack_base[state->ctxstack_ptr];
    if (ctxitem->invocation) {
        for (int i = 0; i < 7; i++)
            lai_var_finalize(&ctxitem->invocation->arg[i]);
        for (int i = 0; i < 8; i++)
            lai_var_finalize(&ctxitem->invocation->local[i]);
        laihost_free(ctxitem->invocation);
    }
    state->ctxstack_ptr -= 1;
}

// --------------------------------------------------------------------------------------
// Inline function for block stack manipulation.
// --------------------------------------------------------------------------------------

static inline int lai_exec_reserve_blkstack(lai_state_t *state) {
    if (state->blkstack_ptr + 1 == state->blkstack_capacity) {
        size_t new_capacity = 2 * state->blkstack_capacity;
        struct lai_blkitem *new_stack = laihost_malloc(new_capacity * sizeof(struct lai_blkitem));
        if (!new_stack) {
            lai_warn("failed to allocate memory for block stack");
            return 1;
        }
        memcpy(new_stack, state->blkstack_base,
               (state->blkstack_ptr + 1) * sizeof(struct lai_blkitem));
        if (state->blkstack_base != state->small_blkstack)
            laihost_free(state->blkstack_base);
        state->blkstack_base = new_stack;
        state->blkstack_capacity = new_capacity;
    }
    return 0;
}

// Pushes a new item to the block stack and returns it.
static inline struct lai_blkitem *lai_exec_push_blkstack(lai_state_t *state) {
    state->blkstack_ptr++;
    // Users are expected to call the reserve() function before this one.
    LAI_ENSURE(state->blkstack_ptr < state->blkstack_capacity);
    memset(&state->blkstack_base[state->blkstack_ptr], 0, sizeof(struct lai_blkitem));
    return &state->blkstack_base[state->blkstack_ptr];
}

// Returns the last item of the block stack.
static inline struct lai_blkitem *lai_exec_peek_blkstack_back(lai_state_t *state) {
    if (state->blkstack_ptr < 0)
        return NULL;
    return &state->blkstack_base[state->blkstack_ptr];
}

// Removes an item from the block stack.
static inline void lai_exec_pop_blkstack_back(lai_state_t *state) {
    LAI_ENSURE(state->blkstack_ptr >= 0);
    state->blkstack_ptr -= 1;
}

// --------------------------------------------------------------------------------------
// Inline function for execution stack manipulation.
// --------------------------------------------------------------------------------------

static inline int lai_exec_reserve_stack(lai_state_t *state) {
    if (state->stack_ptr + 1 == state->stack_capacity) {
        size_t new_capacity = 2 * state->stack_capacity;
        lai_stackitem_t *new_stack = laihost_malloc(new_capacity * sizeof(lai_stackitem_t));
        if (!new_stack) {
            lai_warn("failed to allocate memory for execution stack");
            return 1;
        }
        memcpy(new_stack, state->stack_base,
               (state->stack_ptr + 1) * sizeof(lai_stackitem_t));
        if (state->stack_base != state->small_stack)
            laihost_free(state->stack_base);
        state->stack_base = new_stack;
        state->stack_capacity = new_capacity;
    }
    return 0;
}

// Pushes a new item to the execution stack and returns it.
static inline lai_stackitem_t *lai_exec_push_stack(lai_state_t *state) {
    state->stack_ptr++;
    // Users are expected to call the reserve() function before this one.
    LAI_ENSURE(state->stack_ptr < state->stack_capacity);
    return &state->stack_base[state->stack_ptr];
}

// Returns the n-th item from the top of the stack.
static inline lai_stackitem_t *lai_exec_peek_stack(lai_state_t *state, int n) {
    if (state->stack_ptr - n < 0)
        return NULL;
    return &state->stack_base[state->stack_ptr - n];
}

// Returns the last item of the stack.
static inline lai_stackitem_t *lai_exec_peek_stack_back(lai_state_t *state) {
    return lai_exec_peek_stack(state, 0);
}

// Returns the lai_stackitem_t pointed to by the state's context_ptr.
static inline lai_stackitem_t *lai_exec_peek_stack_at(lai_state_t *state, int n) {
    if (n < 0)
        return NULL;
    return &state->stack_base[n];
}

// Removes the last item from the stack.
static inline void lai_exec_pop_stack_back(lai_state_t *state) {
    LAI_ENSURE(state->stack_ptr >= 0);
    state->stack_ptr--;
}

// --------------------------------------------------------------------------------------
// Inline function for opstack manipulation.
// --------------------------------------------------------------------------------------

static inline int lai_exec_reserve_opstack(lai_state_t *state) {
    if (state->opstack_ptr == state->opstack_capacity) {
        size_t new_capacity = 2 * state->opstack_capacity;
        struct lai_operand *new_stack = laihost_malloc(new_capacity * sizeof(struct lai_operand));
        if (!new_stack) {
            lai_warn("failed to allocate memory for operand stack");
            return 1;
        }
        // TODO: Here, we rely on the fact that moving lai_variable_t via memcpy() is OK.
        //       Implement a some sophisticated lai_operand_move()?
        memcpy(new_stack, state->opstack_base,
               state->opstack_ptr * sizeof(struct lai_operand));
        if (state->opstack_base != state->small_opstack)
            laihost_free(state->opstack_base);
        state->opstack_base = new_stack;
        state->opstack_capacity = new_capacity;
    }
    return 0;
}

static inline int lai_exec_reserve_opstack_n(lai_state_t *state, int n) {
    int err;
    while (n) {
        if ((err = lai_exec_reserve_opstack(state)))
            return err;
        n--;
    }
    return 0;
}

// Pushes a new item to the opstack and returns it.
static inline struct lai_operand *lai_exec_push_opstack(lai_state_t *state) {
    // Users are expected to call the reserve() function before this one.
    LAI_ENSURE(state->opstack_ptr < state->opstack_capacity);
    struct lai_operand *object = &state->opstack_base[state->opstack_ptr];
    memset(object, 0, sizeof(struct lai_operand));
    state->opstack_ptr++;
    return object;
}

// Returns the n-th item from the opstack.
static inline struct lai_operand *lai_exec_get_opstack(lai_state_t *state, int n) {
    LAI_ENSURE(n < state->opstack_ptr);
    return &state->opstack_base[n];
}

// Removes n items from the opstack.
static inline void lai_exec_pop_opstack(lai_state_t *state, int n) {
    for (int k = 0; k < n; k++) {
        struct lai_operand *operand = &state->opstack_base[state->opstack_ptr - k - 1];
        if (operand->tag == LAI_OPERAND_OBJECT)
            lai_var_finalize(&operand->object);
    }
    state->opstack_ptr -= n;
}

// Removes the last item from the opstack.
static inline void lai_exec_pop_opstack_back(lai_state_t *state) {
    lai_exec_pop_opstack(state, 1);
}
