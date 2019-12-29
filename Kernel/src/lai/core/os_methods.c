
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "exec_impl.h"
#include "libc.h"

static const char *lai_emulated_os = "Microsoft Windows NT";        // OS family
static uint64_t lai_implemented_version = 2;                // ACPI 2.0

static const char *supported_osi_strings[] = {
    "Windows 2000",        /* Windows 2000 */
    "Windows 2001",        /* Windows XP */
    "Windows 2001 SP1",    /* Windows XP SP1 */
    "Windows 2001.1",      /* Windows Server 2003 */
    "Windows 2006",        /* Windows Vista */
    "Windows 2006.1",      /* Windows Server 2008 */
    "Windows 2006 SP1",    /* Windows Vista SP1 */
    "Windows 2006 SP2",    /* Windows Vista SP2 */
    "Windows 2009",        /* Windows 7 */
    "Windows 2012",        /* Windows 8 */
    "Windows 2013",        /* Windows 8.1 */
    "Windows 2015",        /* Windows 10 */
};

// Pretend to be windows when we execute the OSI() method.
int lai_do_osi_method(lai_variable_t *args, lai_variable_t *result) {
    const char *query = lai_exec_string_access(&args[0]);

    uint32_t osi_return = 0;
    for (size_t i = 0; i < (sizeof(supported_osi_strings) / sizeof(uint64_t)); i++) {
        if (!lai_strcmp(query, supported_osi_strings[i])) {
            osi_return = 0xFFFFFFFF;
            break;
        }
    }

    if (!osi_return && !lai_strcmp(query, "Linux"))
        lai_warn("buggy BIOS requested _OSI('Linux'), ignoring...");

    result->type = LAI_INTEGER;
    result->integer = osi_return;

    lai_debug("_OSI('%s') returned %08X", query, osi_return);
    return 0;
}

// same for both of the functions below.
int lai_do_os_method(lai_variable_t *args, lai_variable_t *result) {
    if (lai_create_c_string(result, lai_emulated_os))
        lai_panic("could not allocate memory for string");
    lai_debug("_OS_ returned '%s'", lai_emulated_os);
    return 0;
}

int lai_do_rev_method(lai_variable_t *args, lai_variable_t *result) {
    result->type = LAI_INTEGER;
    result->integer = lai_implemented_version;

    lai_debug("_REV returned %d", result->integer);
    return 0;
}
