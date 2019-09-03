/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "libc.h"
#include "exec_impl.h"

int lai_create_string(lai_variable_t *object, size_t length) {
    object->type = LAI_STRING;
    object->string_ptr = laihost_malloc(sizeof(struct lai_string_head));
    if (!object->string_ptr)
        return 1;
    object->string_ptr->rc = 1;
    object->string_ptr->content = laihost_malloc(length + 1);
    if (!object->string_ptr->content) {
        laihost_free(object->string_ptr);
        return 1;
    }
    memset(object->string_ptr->content, 0, length + 1);
    return 0;
}

int lai_create_c_string(lai_variable_t *object, const char *s) {
    size_t n = lai_strlen(s);
    int e;
    e = lai_create_string(object, n);
    if(e)
        return e;
    memcpy(lai_exec_string_access(object), s, n);
    return 0;
}

int lai_create_buffer(lai_variable_t *object, size_t size) {
    object->type = LAI_BUFFER;
    object->buffer_ptr = laihost_malloc(sizeof(struct lai_buffer_head));
    if (!object->buffer_ptr)
        return 1;
    object->buffer_ptr->rc = 1;
    object->buffer_ptr->size = size;
    object->buffer_ptr->content = laihost_malloc(size);
    if (!object->buffer_ptr->content) {
        laihost_free(object->buffer_ptr);
        return 1;
    }
    memset(object->buffer_ptr->content, 0, size);
    return 0;
}

int lai_create_pkg(lai_variable_t *object, size_t n) {
    object->type = LAI_PACKAGE;
    object->pkg_ptr = laihost_malloc(sizeof(struct lai_pkg_head));
    if (!object->pkg_ptr)
        return 1;
    object->pkg_ptr->rc = 1;
    object->pkg_ptr->size = n;
    object->pkg_ptr->elems = laihost_malloc(n * sizeof(lai_variable_t));
    if (!object->pkg_ptr->elems) {
        laihost_free(object->pkg_ptr);
        return 1;
    }
    memset(object->pkg_ptr->elems, 0, n * sizeof(lai_variable_t));
    return 0;
}

lai_api_error_t lai_obj_resize_string(lai_variable_t *object, size_t length) {
    if (object->type != LAI_STRING)
        return LAI_ERROR_TYPE_MISMATCH;
    if (length > lai_strlen(object->string_ptr->content)) {
        char *new_content = laihost_malloc(length + 1);
        if (!new_content)
            return LAI_ERROR_OUT_OF_MEMORY;
        lai_strcpy(new_content, object->string_ptr->content);
        laihost_free(object->string_ptr->content);
        object->string_ptr->content = new_content;
    }
    return LAI_ERROR_NONE;
}

lai_api_error_t lai_obj_resize_buffer(lai_variable_t *object, size_t size) {
    if (object->type != LAI_BUFFER)
        return LAI_ERROR_TYPE_MISMATCH;
    if (size > object->buffer_ptr->size) {
        uint8_t *new_content = laihost_malloc(size);
        if (!new_content)
            return LAI_ERROR_OUT_OF_MEMORY;
        memset(new_content, 0, size);
        memcpy(new_content, object->buffer_ptr->content, object->buffer_ptr->size);
        laihost_free(object->buffer_ptr->content);
        object->buffer_ptr->content = new_content;
    }
    object->buffer_ptr->size = size;
    return LAI_ERROR_NONE;
}

lai_api_error_t lai_obj_resize_pkg(lai_variable_t *object, size_t n) {
    if (object->type != LAI_PACKAGE)
        return LAI_ERROR_TYPE_MISMATCH;
    if (n <= object->pkg_ptr->size) {
        for (unsigned int i = n; i < object->pkg_ptr->size; i++)
            lai_var_finalize(&object->pkg_ptr->elems[i]);
    } else {
        struct lai_variable_t *new_elems = laihost_malloc(n * sizeof(lai_variable_t));
        if (!new_elems)
            return LAI_ERROR_OUT_OF_MEMORY;
        memset(new_elems, 0, n * sizeof(lai_variable_t));
        for (unsigned int i = 0; i < object->pkg_ptr->size; i++)
            lai_var_move(&new_elems[i], &object->pkg_ptr->elems[i]);
        laihost_free(object->pkg_ptr->elems);
        object->pkg_ptr->elems = new_elems;
    }
    object->pkg_ptr->size = n;
    return LAI_ERROR_NONE;
}

static enum lai_object_type lai_object_type_of_objref(lai_variable_t *object) {
    switch (object->type) {
        case LAI_INTEGER:
            return LAI_TYPE_INTEGER;
        case LAI_STRING:
            return LAI_TYPE_STRING;
        case LAI_BUFFER:
            return LAI_TYPE_BUFFER;
        case LAI_PACKAGE:
            return LAI_TYPE_PACKAGE;

        default:
            lai_panic("unexpected object type %d in lai_object_type_of_objref()", object->type);
    }
}

static enum lai_object_type lai_object_type_of_node(lai_nsnode_t *handle) {
    switch (handle->type) {
        case LAI_NAMESPACE_DEVICE:
            return LAI_TYPE_DEVICE;

        default:
            lai_panic("unexpected node type %d in lai_object_type_of_node()", handle->type);
    }
}

enum lai_object_type lai_obj_get_type(lai_variable_t *object) {
    switch (object->type) {
        case LAI_INTEGER:
        case LAI_STRING:
        case LAI_BUFFER:
        case LAI_PACKAGE:
            return lai_object_type_of_objref(object);

        case LAI_HANDLE:
            return lai_object_type_of_node(object->handle);
        case LAI_LAZY_HANDLE: {
            struct lai_amlname amln;
            lai_amlname_parse(&amln, object->unres_aml);

            lai_nsnode_t *handle = lai_do_resolve(object->unres_ctx_handle, &amln);
            if(!handle)
                lai_panic("undefined reference %s", lai_stringify_amlname(&amln));
            return lai_object_type_of_node(handle);
        }
        case 0:
            return LAI_TYPE_NONE;
        default:
            lai_panic("unexpected object type %d for lai_obj_get_type()", object->type);
    }
}

lai_api_error_t lai_obj_get_integer(lai_variable_t *object, uint64_t *out) {
    switch (object->type) {
        case LAI_INTEGER:
            *out = object->integer;
            return LAI_ERROR_NONE;

        default:
            lai_warn("lai_obj_get_integer() expects an integer, not a value of type %d",
                      object->type);
            return LAI_ERROR_TYPE_MISMATCH;
    }
}

lai_api_error_t lai_obj_get_pkg(lai_variable_t *object, size_t i, lai_variable_t *out) {
    if (object->type != LAI_PACKAGE)
        return LAI_ERROR_TYPE_MISMATCH;
    if (i >= lai_exec_pkg_size(object))
        return LAI_ERROR_OUT_OF_BOUNDS;
    lai_exec_pkg_load(out, object, i);
    return 0;
}

lai_api_error_t lai_obj_get_handle(lai_variable_t *object, lai_nsnode_t **out) {
    switch (object->type) {
        case LAI_HANDLE:
            *out = object->handle;
            return LAI_ERROR_NONE;
        case LAI_LAZY_HANDLE: {
            struct lai_amlname amln;
            lai_amlname_parse(&amln, object->unres_aml);

            lai_nsnode_t *handle = lai_do_resolve(object->unres_ctx_handle, &amln);
            if(!handle)
                lai_panic("undefined reference %s", lai_stringify_amlname(&amln));
            *out = handle;
            return LAI_ERROR_NONE;
        }

        default:
            lai_warn("lai_obj_get_handle() expects a handle type, not a value of type %d",
                      object->type);
            return LAI_ERROR_TYPE_MISMATCH;
    }
}

lai_api_error_t lai_obj_to_buffer(lai_variable_t *out, lai_variable_t *object){
    switch (object->type)
    {
    case LAI_TYPE_INTEGER:
        if(lai_create_buffer(out, sizeof(uint64_t)))
            return LAI_ERROR_OUT_OF_MEMORY;
        memcpy(out->buffer_ptr->content, &object->integer, sizeof(uint64_t));
        break;

    case LAI_TYPE_BUFFER:
        lai_obj_clone(out, object);
        break;
    
    case LAI_TYPE_STRING: {
        size_t len = lai_exec_string_length(object);
        if(len == 0){
            if(lai_create_buffer(out, 0))
                return LAI_ERROR_OUT_OF_MEMORY;
        } else {
            if(lai_create_buffer(out, len + 1))
                return LAI_ERROR_OUT_OF_MEMORY;
            memcpy(out->buffer_ptr->content, object->string_ptr->content, len);
        }
        break;
    }
    
    default:
        lai_warn("lai_obj_to_buffer() unsupported object type %d",
                   object->type);
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }

    return LAI_ERROR_NONE;
}

lai_api_error_t lai_obj_to_string(lai_variable_t *out, lai_variable_t *object, size_t size){
    switch (object->type)
    {
    case LAI_TYPE_BUFFER: {
        size_t buffer_length = 0;
        uint8_t *buffer = lai_exec_buffer_access(object);
        for(uint64_t i = 0; i < lai_exec_buffer_size(object); i++){
            if(buffer[i] == '\0') 
                break;
            buffer_length++;
        }
        
        if(buffer_length == 0){
            lai_create_string(out, 0);
        } else if(size == ~(uint64_t)(0)){
            // Copy until the '\0'
            lai_create_string(out, buffer_length + 1);
            char *string = lai_exec_string_access(out);
            memcpy(string, buffer, buffer_length);
        } else {
            if(size < buffer_length){
                lai_create_string(out, size);
                char *string = lai_exec_string_access(out);
                memcpy(string, buffer, size);
            } else {
                lai_create_string(out, buffer_length);
                char *string = lai_exec_string_access(out);
                memcpy(string, buffer, buffer_length);
            }
            
        }
        break;
    }
    
    default:
        lai_warn("lai_obj_to_string() unsupported object type %d",
                   object->type);
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }

    return LAI_ERROR_NONE;
}

lai_api_error_t lai_obj_to_decimal_string(lai_variable_t *out, lai_variable_t *object){
    switch (object->type)
    {
    case LAI_BUFFER: {
        size_t buffer_len = lai_exec_buffer_size(object);
        uint8_t *buffer = lai_exec_buffer_access(object);
        lai_create_string(out, (buffer_len * 3)); // For every buffer byte we need 2 chars of number and a comma

        char *string = lai_exec_string_access(out);
        uint64_t string_index = 0;

        for(uint64_t i = 0; i < buffer_len; i++){
            char buf[5] = "";
            lai_snprintf(buf, 5, "%02d", buffer[i]);

            string[string_index] = buf[0];
            string[string_index + 1] = buf[1];
            string[string_index + 2] = ',';
            string_index += 3;
        }
        // String with values should be constructed now, remove the last comma
        string[string_index - 1] = '\0';
        break;
    }

    case LAI_STRING:
        lai_obj_clone(out, object);
        break;
    
    default:
        lai_warn("lai_obj_to_decimal_string() unsupported object type %d",
                   object->type);
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }

    return LAI_ERROR_NONE;
}

// The spec doesn't mention this but the numbers should be prefixed with 0x
lai_api_error_t lai_obj_to_hex_string(lai_variable_t *out, lai_variable_t *object){
    switch (object->type)
    {
    case LAI_BUFFER: {
        size_t buffer_len = lai_exec_buffer_size(object);
        uint8_t *buffer = lai_exec_buffer_access(object);
        lai_create_string(out, (buffer_len * 5)); // For every buffer byte we need 2 chars of prefix, 2 chars of number and a comma
                                                  // I'll take the 1 byte loss of the last comma for code simplicity

        char *string = lai_exec_string_access(out);
        uint64_t string_index = 0;

        for(uint64_t i = 0; i < buffer_len; i++){
            char buf[5] = "";
            lai_snprintf(buf, 5, "%02X", buffer[i]);

            string[string_index] = '0';
            string[string_index + 1] = 'x';
            string[string_index + 2] = buf[0];
            string[string_index + 3] = buf[1];
            string[string_index + 4] = ',';
            string_index += 5;
        }
        // String with values should be constructed now, remove the last comma
        string[string_index - 1] = '\0';
        break;
    }

    case LAI_STRING:
        lai_obj_clone(out, object);
        break;
    
    default:
        lai_warn("lai_obj_to_decimal_string() unsupported object type %d",
                   object->type);
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }

    return LAI_ERROR_NONE;
}

lai_api_error_t lai_obj_to_integer(lai_variable_t *out, lai_variable_t *object){
    switch (object->type)
    {

    case LAI_BUFFER: {
        size_t buffer_len = lai_exec_buffer_size(object);
        uint64_t *buffer = lai_exec_buffer_access(object);

        if (buffer_len < 8) {
            lai_warn("lai_obj_to_integer() buffer shorter than 8 bytes");
            return LAI_ERROR_ILLEGAL_ARGUMENTS;
        }

        out->type = LAI_INTEGER;
        out->integer = *buffer;

        #ifndef __BYTE_ORDER__
            #error Required macro __BYTE_ORDER__ not defined
        #endif
        #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            out->integer = bswap64(out->integer);
        #endif

        break;
    }

    case LAI_STRING: {
        size_t string_len = lai_exec_string_length(object);
        const char *string = lai_exec_string_access(object);

        uint64_t integer = 0;

        // Check if hexadecimal
        if (string_len >= 2 && string[0] == '0' && (string[1] == 'x' || string[1] == 'X')) {
            for (size_t i = 2; i < string_len; i++) {
                unsigned v;
                if      (string[i] >= '0' && string[i] <= '9')
                    v = string[i] - '0';
                else if (string[i] >= 'a' && string[i] <= 'f')
                    v = string[i] - 'a' + 10;
                else if (string[i] >= 'A' && string[i] <= 'F')
                    v = string[i] - 'A' + 10;
                else {
                    lai_warn("lai_obj_to_integer() hexadecimal string contains non valid character %c",
                              string[i]);
                    return LAI_ERROR_ILLEGAL_ARGUMENTS;
                }
                integer = integer * 16 + v;
            }
        } else {
            for (size_t i = 0; i < string_len; i++) {
                if (string[i] < '0' || string[i] > '9') {
                    lai_warn("lai_obj_to_integer() decimal string contains non valid character %c",
                              string[i]);
                    return LAI_ERROR_ILLEGAL_ARGUMENTS;
                }
                integer = integer * 10 + (string[i] - '0');
            }
        }

        out->type = LAI_INTEGER;
        out->integer = integer;

        break;
    }

    case LAI_INTEGER:
        lai_obj_clone(out, object);
        break;

    default:
        lai_warn("lai_obj_to_integer() unsupported object type %d",
                   object->type);
        return LAI_ERROR_ILLEGAL_ARGUMENTS;
    }

    return LAI_ERROR_NONE;
}

// lai_clone_buffer(): Clones a buffer object
static void lai_clone_buffer(lai_variable_t *dest, lai_variable_t *source) {
    size_t size = lai_exec_buffer_size(source);
    if (lai_create_buffer(dest, size))
        lai_panic("unable to allocate memory for buffer object.");
    memcpy(lai_exec_buffer_access(dest), lai_exec_buffer_access(source), size);
}

// lai_clone_string(): Clones a string object
static void lai_clone_string(lai_variable_t *dest, lai_variable_t *source) {
    size_t n = lai_exec_string_length(source);
    if (lai_create_string(dest, n))
        lai_panic("unable to allocate memory for string object.");
    memcpy(lai_exec_string_access(dest), lai_exec_string_access(source), n);
}

// lai_clone_package(): Clones a package object
static void lai_clone_package(lai_variable_t *dest, lai_variable_t *src) {
    size_t n = src->pkg_ptr->size;
    if (lai_create_pkg(dest, n))
        lai_panic("unable to allocate memory for package object.");
    for (int i = 0; i < n; i++)
        lai_obj_clone(&dest->pkg_ptr->elems[i], &src->pkg_ptr->elems[i]);
}

// lai_obj_clone(): Copies an object
void lai_obj_clone(lai_variable_t *dest, lai_variable_t *source) {
    // Clone into a temporary object.
    lai_variable_t temp = {0};
    switch (source->type) {
        case LAI_STRING:
            lai_clone_string(&temp, source);
            break;
        case LAI_BUFFER:
            lai_clone_buffer(&temp, source);
            break;
        case LAI_PACKAGE:
            lai_clone_package(&temp, source);
            break;
    }

    if (temp.type) {
        // Afterwards, swap to the destination. This handles copy-to-self correctly.
        lai_swap_object(dest, &temp);
        lai_var_finalize(&temp);
    }else{
        // For others objects: just do a shallow copy.
        lai_var_assign(dest, source);
    }
}
