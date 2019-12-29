
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

// LAI internal header

void *lai_calloc(size_t, size_t);
size_t lai_strlen(const char *);
char *lai_strcpy(char *, const char *);
int lai_strcmp(const char *, const char *);

void lai_vsnprintf(char *, size_t, const char *, va_list);
void lai_snprintf(char *, size_t, const char *, ...);
