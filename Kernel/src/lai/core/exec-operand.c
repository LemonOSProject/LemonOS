/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "libc.h"
#include "opregion.h"
#include "exec_impl.h"
#include "ns_impl.h"

size_t lai_exec_string_length(lai_variable_t *str) {
    LAI_ENSURE(str->type == LAI_STRING);
    return lai_strlen(str->string_ptr->content);
}

// Note: This function exists to enable better GC and proper locking in the future.
void lai_exec_pkg_var_load(lai_variable_t *out, struct lai_pkg_head *head, size_t i) {
    lai_var_assign(out, &head->elems[i]);
}

// Note: This function exists to enable better GC and proper locking in the future.
void lai_exec_pkg_var_store(lai_variable_t *in, struct lai_pkg_head *head, size_t i) {
    lai_var_assign(&head->elems[i], in);
}

static void lai_write_buffer(lai_nsnode_t *handle, lai_variable_t *source);

// --------------------------------------------------------------------------------------
// Helpers for load/store operations.
// --------------------------------------------------------------------------------------

void lai_exec_ref_load(lai_variable_t *object, lai_variable_t *ref) {
    // Note: This function intentionally *does not* handle indices.
    switch (ref->type) {
        case LAI_ARG_REF:
            lai_var_assign(object, &ref->iref_invocation->arg[ref->iref_index]);
            break;
        case LAI_LOCAL_REF:
            lai_var_assign(object, &ref->iref_invocation->local[ref->iref_index]);
            break;
        case LAI_NODE_REF:
            lai_exec_access(object, ref->handle);
            break;
        default:
            lai_panic("unknown reference type %d for lai_exec_ref_load()", ref->type);
    }
}

void lai_exec_ref_store(lai_variable_t *ref, lai_variable_t *object) {
    // Note: This function intentionally *does not* handle indices.
    switch (ref->type) {
        case LAI_ARG_REF:
            lai_var_assign(&ref->iref_invocation->arg[ref->iref_index], object);
            break;
        case LAI_LOCAL_REF:
            lai_var_assign(&ref->iref_invocation->local[ref->iref_index], object);
            break;
        case LAI_NODE_REF:
            lai_store_ns(ref->handle, object);
            break;
        default:
            lai_panic("unknown reference type %d for lai_exec_ref_store()", ref->type);
    }
}

// --------------------------------------------------------------------------------------
// lai_exec_access().
// --------------------------------------------------------------------------------------

// lai_exec_access() loads an object from a namespace node.
void lai_exec_access(lai_variable_t *object, lai_nsnode_t *src) {
    switch (src->type) {
        case LAI_NAMESPACE_NAME:
            lai_var_assign(object, &src->object);
            break;
        case LAI_NAMESPACE_FIELD:
        case LAI_NAMESPACE_INDEXFIELD:
            lai_read_opregion(object, src);
            break;
        case LAI_NAMESPACE_DEVICE:
            object->type = LAI_HANDLE;
            object->handle = src;
            break;
        default:
            lai_panic("unexpected type %d of named object in lai_exec_access()", src->type);
    }
}

// --------------------------------------------------------------------------------------
// lai_operand_load() and lai_operand_store().
// --------------------------------------------------------------------------------------

/* lai_operand_load() / lai_operand_store() perform loads / stores of operands.
 * In this sense, they are complimentary. HOWEVER, the operations are asymmetric in subtle ways!
 * Furthermore, the functions are also implemented and called in an asymmetric way:
 * lai_operand_store() is often called during operator reductions; on the other hand,
 * it is almost never necessary to call lai_operand_load() as the interpreter usually
 * handles loads directly via lai_exec_access(). */

void lai_operand_load(lai_state_t *state, struct lai_operand *src, lai_variable_t *object) {
    switch (src->tag) {
        case LAI_ARG_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);
            lai_var_assign(object, &ctxitem->invocation->arg[src->index]);
            break;
        }
        case LAI_LOCAL_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);
            lai_var_assign(object, &ctxitem->invocation->local[src->index]);
            break;
        }
        case LAI_RESOLVED_NAME:
            lai_exec_access(object, src->handle);
            break;
        default:
            lai_panic("tag %d is not valid for lai_load()", src->tag);
    }
}

void lai_store_ns(lai_nsnode_t *target, lai_variable_t *object) {
    switch (target->type) {
        case LAI_NAMESPACE_NAME:
            lai_var_assign(&target->object, object);
            break;
        case LAI_NAMESPACE_FIELD:
        case LAI_NAMESPACE_INDEXFIELD:
            lai_write_opregion(target, object);
            break;
        case LAI_NAMESPACE_BUFFER_FIELD:
            lai_write_buffer(target, object);
            break;
        default:
            lai_panic("unexpected type %d of named object in lai_store_ns()");
    }
}

void lai_exec_mutate_ns(lai_nsnode_t *target, lai_variable_t *object) {
    switch (target->type) {
        case LAI_NAMESPACE_NAME:
            switch (target->object.type) {
                case LAI_STRING: {
                    // Strings are resized during mutation.
                    LAI_ENSURE(object->type == LAI_STRING); // TODO: Implement conversion.
                    size_t length = lai_strlen(lai_exec_string_access(object));
                    if (lai_obj_resize_string(&target->object, length))
                        lai_panic("could not resize string in lai_exec_mutate_ns()");
                    lai_strcpy(lai_exec_string_access(&target->object),
                           lai_exec_string_access(object));
                    break;
                }
                case LAI_BUFFER: {
                    // Buffers are *not* resized during mutation.
                    // The target buffer determines the size of the result.
                    LAI_ENSURE(object->type == LAI_BUFFER); // TODO: Implement conversion.
                    size_t copy_size = lai_exec_buffer_size(object);
                    size_t buffer_size = lai_exec_buffer_size(&target->object);
                    if (copy_size > buffer_size)
                        copy_size = buffer_size;
                    memset(lai_exec_buffer_access(&target->object), 0, buffer_size);
                    memcpy(lai_exec_buffer_access(&target->object),
                           lai_exec_buffer_access(object), copy_size);
                    break;
                }
                case LAI_PACKAGE: {
                    // Packages are resized during mutation.
                    LAI_ENSURE(object->type == LAI_PACKAGE); // TODO: Implement conversion.
                    size_t n = lai_exec_pkg_size(object);
                    if (lai_obj_resize_pkg(&target->object, n))
                        lai_panic("could not resize package in lai_exec_mutate_ns()");
                    for (size_t i = 0; i < n; i++) {
                        lai_variable_t temp = LAI_VAR_INITIALIZER;
                        lai_exec_pkg_load(&temp, object, i);
                        lai_exec_pkg_store(&temp, &target->object, i);
                    }
                    break;
                }
                default:
                    lai_var_assign(&target->object, object);
            }
            break;
        case LAI_NAMESPACE_FIELD:
        case LAI_NAMESPACE_INDEXFIELD:
            lai_write_opregion(target, object);
            break;
        case LAI_NAMESPACE_BUFFER_FIELD:
            lai_write_buffer(target, object);
            break;
        default:
            lai_panic("unexpected type %d of named object in lai_exec_mutate_ns()");
    }
}

// lai_operand_mutate(): Modifies the operand in-place to store the object.
//                       This is the type of store used by Store() and arithmetic operators.
void lai_operand_mutate(lai_state_t *state,
                        struct lai_operand *dest, lai_variable_t *object) {
    // First, handle stores to AML references (returned by Index() and friends).
    if (dest->tag == LAI_OPERAND_OBJECT) {
        switch (dest->object.type) {
            case LAI_STRING_INDEX: {
                char *window = dest->object.string_ptr->content;
                window[dest->object.integer] = object->integer;
                break;
            }
            case LAI_BUFFER_INDEX: {
                uint8_t *window = dest->object.buffer_ptr->content;
                window[dest->object.integer] = object->integer;
                break;
            }
            case LAI_PACKAGE_INDEX: {
                lai_variable_t copy = {0};
                lai_var_assign(&copy, object);
                lai_exec_pkg_var_store(&copy, dest->object.pkg_ptr, dest->object.integer);
                lai_var_finalize(&copy);
                break;
            }
            default:
                lai_panic("unexpected object type %d for lai_store_overwrite()", dest->object.type);
        }
        return;
    }

    switch (dest->tag) {
        case LAI_NULL_NAME:
            // Stores to the null target are ignored.
            break;
        case LAI_RESOLVED_NAME:
            lai_exec_mutate_ns(dest->handle, object);
            break;
        case LAI_ARG_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);

            // Stores to ARGx that contain references automatically dereference ARGx
            // and store to the target of the reference instead.
            lai_variable_t *arg_var = &ctxitem->invocation->arg[dest->index];
            switch (arg_var->type) {
                case LAI_ARG_REF:
                case LAI_LOCAL_REF:
                case LAI_NODE_REF:
                    lai_exec_ref_store(arg_var, object);
                    break;
                default:
                    lai_var_assign(arg_var, object);
            };
            break;
        }
        case LAI_LOCAL_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);
            lai_var_assign(&ctxitem->invocation->local[dest->index], object);
            break;
        }
        case LAI_DEBUG_NAME:
            if(laihost_handle_amldebug)
                laihost_handle_amldebug(object);
            else {
                switch (object->type) {
                    case LAI_INTEGER:
                        lai_debug("Debug(): integer(%ld)", object->integer);
                        break;
                    case LAI_STRING:
                        lai_debug("Debug(): string(\"%s\")", lai_exec_string_access(object));
                        break;
                    case LAI_BUFFER:
                        lai_debug("Debug(): buffer(%X)", (size_t)lai_exec_buffer_access(object));
                        break;
                    default:
                        lai_debug("Debug(): type %d", object->type);
                }
            }
            break;
        default:
            lai_panic("tag %d is not valid for lai_store_overwrite()", dest->tag);
    }
}

// lai_operand_emplace(): Stores the object to the operand, replacing the current contents.
//                        This is used by CopyObject() and type conversion operators.
void lai_operand_emplace(lai_state_t *state,
                         struct lai_operand *dest, lai_variable_t *object) {
    // First, handle stores to AML references (returned by Index() and friends).
    if (dest->tag == LAI_OPERAND_OBJECT) {
        switch (dest->object.type) {
            case LAI_STRING_INDEX: {
                char *window = dest->object.string_ptr->content;
                window[dest->object.integer] = object->integer;
                break;
            }
            case LAI_BUFFER_INDEX: {
                uint8_t *window = dest->object.buffer_ptr->content;
                window[dest->object.integer] = object->integer;
                break;
            }
            case LAI_PACKAGE_INDEX: {
                lai_variable_t copy = {0};
                lai_var_assign(&copy, object);
                lai_exec_pkg_var_store(&copy, dest->object.pkg_ptr, dest->object.integer);
                lai_var_finalize(&copy);
                break;
            }
            default:
                lai_panic("unexpected object type %d for lai_store_overwrite()", dest->object.type);
        }
        return;
    }

    switch (dest->tag) {
        case LAI_NULL_NAME:
            // Stores to the null target are ignored.
            break;
        case LAI_RESOLVED_NAME:
            lai_store_ns(dest->handle, object);
            break;
        case LAI_ARG_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);

            // Stores to ARGx that contain references automatically dereference ARGx
            // and store to the target of the reference instead.
            lai_variable_t *arg_var = &ctxitem->invocation->arg[dest->index];
            switch (arg_var->type) {
                case LAI_ARG_REF:
                case LAI_LOCAL_REF:
                case LAI_NODE_REF:
                    lai_exec_ref_store(arg_var, object);
                    break;
                default:
                    lai_var_assign(arg_var, object);
            };
            break;
        }
        case LAI_LOCAL_NAME: {
            struct lai_ctxitem *ctxitem = lai_exec_peek_ctxstack_back(state);
            LAI_ENSURE(ctxitem->invocation);
            lai_var_assign(&ctxitem->invocation->local[dest->index], object);
            break;
        }
        case LAI_DEBUG_NAME:
            if(laihost_handle_amldebug)
                laihost_handle_amldebug(object);
            else {
                switch (object->type) {
                    case LAI_INTEGER:
                        lai_debug("Debug(): integer(%ld)", object->integer);
                        break;
                    case LAI_STRING:
                        lai_debug("Debug(): string(\"%s\")", lai_exec_string_access(object));
                        break;
                    case LAI_BUFFER:
                        lai_debug("Debug(): buffer(%X)", (size_t)lai_exec_buffer_access(object));
                        break;
                    default:
                        lai_debug("Debug(): type %d", object->type);
                }
            }
            break;
        default:
            lai_panic("tag %d is not valid for lai_store_overwrite()", dest->tag);
    }
}

// Load an object (i.e., integer, string, buffer, package) or reference.
// This is the access method used by Store().
// Returns immediate objects and indices as-is (i.e., without load from a name).
// Returns a view of an existing object and not a clone of the object.
void lai_exec_get_objectref(lai_state_t *state, struct lai_operand *src, lai_variable_t *object) {
    LAI_ENSURE(src->tag == LAI_OPERAND_OBJECT);
    lai_var_assign(object, &src->object);
}

// Load an integer.
// Returns immediate objects as-is.
void lai_exec_get_integer(lai_state_t *state, struct lai_operand *src, lai_variable_t *object) {
    LAI_ENSURE(src->tag == LAI_OPERAND_OBJECT);
    lai_variable_t temp = {0};
    lai_var_assign(&temp, &src->object);
    if(temp.type != LAI_INTEGER)
        lai_panic("lai_load_integer() expects an integer, not a value of type %d", temp.type);
    lai_var_move(object, &temp);
}

// lai_write_buffer(): Writes to a BufferField.
static void lai_write_buffer(lai_nsnode_t *handle, lai_variable_t *source) {
    lai_nsnode_t *buffer_handle = handle->bf_node;

    uint64_t value = source->integer;

    // Offset that we are writing to, in bytes.
    size_t offset = handle->bf_offset;
    size_t size = handle->bf_size;
    uint8_t *data = lai_exec_buffer_access(&buffer_handle->object);

    int n = 0; // Number of bits that have been written.
    while (n < size) {
        // First bit (of the current byte) that will be overwritten.
        int bit = (offset + n) & 7;

        // Number of bits (of the current byte) that will be overwritten.
        int m = size - n;
        if (m > (8 - bit))
            m = 8 - bit;
        LAI_ENSURE(m); // Write at least one bit.

        uint8_t mask = (1 << m) - 1;
        data[(offset + n) >> 3] &= ~(mask << bit);
        data[(offset + n) >> 3] |= ((value >> n) & mask) << bit;

        n += m;
    }
}
