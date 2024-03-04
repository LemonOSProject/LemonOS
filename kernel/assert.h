#include <panic.h>

#define assert(expr)                                                                               \
    (expr || (lemon_panic("Assertion failed @ " __FILE__ ", '" #expr "'"), 0))
