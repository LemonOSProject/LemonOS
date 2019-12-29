/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>

const char *lai_api_error_to_string(lai_api_error_t error){
    switch (error) {
    case LAI_ERROR_NONE:
        return "No error";
    case LAI_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case LAI_ERROR_TYPE_MISMATCH:
        return "Type mismatch";
    case LAI_ERROR_OUT_OF_BOUNDS:
        return "Out of bounds";
    case LAI_ERROR_EXECUTION_FAILURE:
        return "Execution failure";
    case LAI_ERROR_ILLEGAL_ARGUMENTS:
        return "Illegal arguments";
    case LAI_ERROR_UNEXPECTED_RESULT:
        return "Unexpected results";
    case LAI_ERROR_END_REACHED:
        return "End of iteration";
    default:
        return "Unknown error";
    }
}
