
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "libc.h"

void lai_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[128 + 16];
    lai_vsnprintf(buf, 128, fmt, args);
    if(laihost_log)
        laihost_log(LAI_DEBUG_LOG, buf);
    va_end(args);
}

void lai_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[128 + 16];
    lai_vsnprintf(buf, 128, fmt, args);
    if(laihost_log)
        laihost_log(LAI_WARN_LOG, buf);
    va_end(args);
}

void lai_panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[128 + 16];
    lai_vsnprintf(buf, 128, fmt, args);
    if(laihost_panic)
        laihost_panic(buf);
    else
    {
        // If the panic function is undefined, try to log the error and abort.
        if(laihost_log)
            laihost_log(LAI_WARN_LOG, buf);
        __builtin_trap();
    }
}

void *lai_calloc(size_t count, size_t item_size) {
    size_t size = count * item_size;
    void *p = laihost_malloc(size);
    if(p)
        memset(p, 0, size);
    return p;
}

size_t lai_strlen(const char *s) {
    size_t len = 0;
    for(size_t i = 0; s[i]; i++)
        len++;
    return len;
}

char *lai_strcpy(char *dest, const char *src) {
    char *dest_it = (char *)dest;
    const char *src_it = (const char *)src;
    while(*src_it)
        *(dest_it++) = *(src_it++);
    *dest_it = 0;
    return dest;
}

int lai_strcmp(const char *a, const char *b) {
    size_t i = 0;
    while(1) {
        unsigned char ac = a[i];
        unsigned char bc = b[i];
        if(!ac && !bc)
            return 0;
        // If only one char is null, one of the following cases applies.
        if(ac < bc)
            return -1;
        if(ac > bc)
            return 1;
        i++;
    }
}

void lai_snprintf(char * buf, size_t bufsize, const char * fmt, ...){
    va_list list;
    va_start(list, fmt);
    lai_vsnprintf(buf, bufsize, fmt, list);
    va_end(list);
}