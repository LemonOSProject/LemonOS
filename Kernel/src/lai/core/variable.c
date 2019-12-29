/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "libc.h"
#include "exec_impl.h"

// laihost_free_package(): Frees a package object and all its children
static void laihost_free_package(lai_variable_t *object) {
    for(int i = 0; i < object->pkg_ptr->size; i++)
        lai_var_finalize(&object->pkg_ptr->elems[i]);
    laihost_free(object->pkg_ptr->elems);
    laihost_free(object->pkg_ptr);
}

void lai_var_finalize(lai_variable_t *object) {
    switch (object->type) {
        case LAI_STRING:
        case LAI_STRING_INDEX:
            if (lai_rc_unref(&object->string_ptr->rc)) {
                laihost_free(object->string_ptr->content);
                laihost_free(object->string_ptr);
            }
            break;
        case LAI_BUFFER:
        case LAI_BUFFER_INDEX:
            if (lai_rc_unref(&object->buffer_ptr->rc)) {
                laihost_free(object->buffer_ptr->content);
                laihost_free(object->buffer_ptr);
            }
            break;
        case LAI_PACKAGE:
        case LAI_PACKAGE_INDEX:
            if (lai_rc_unref(&object->pkg_ptr->rc))
                laihost_free_package(object);
            break;
    }

    memset(object, 0, sizeof(lai_variable_t));
}

// Helper function for lai_var_move() and lai_obj_clone().
void lai_swap_object(lai_variable_t *first, lai_variable_t *second) {
    lai_variable_t temp = *first;
    *first = *second;
    *second = temp;
}

// lai_var_move(): Moves an object: instead of making a deep copy,
void lai_var_move(lai_variable_t *destination, lai_variable_t *source) {
    // Move-by-swap idiom. This handles move-to-self operations correctly.
    lai_variable_t temp = {0};
    lai_swap_object(&temp, source);
    lai_swap_object(&temp, destination);
    lai_var_finalize(&temp);
}

void lai_var_assign(lai_variable_t *dest, lai_variable_t *src) {
    // Make a local shallow copy of the AML object.
    lai_variable_t temp = *src;
    switch (src->type) {
        case LAI_STRING:
        case LAI_STRING_INDEX:
            lai_rc_ref(&src->string_ptr->rc);
            break;
        case LAI_BUFFER:
        case LAI_BUFFER_INDEX:
            lai_rc_ref(&src->buffer_ptr->rc);
            break;
        case LAI_PACKAGE:
        case LAI_PACKAGE_INDEX:
            lai_rc_ref(&src->pkg_ptr->rc);
            break;
    }

    lai_var_move(dest, &temp);
}
