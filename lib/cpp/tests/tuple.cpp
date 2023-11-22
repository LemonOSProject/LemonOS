#include "../include/tuple.h"

#include <assert.h>

constexpr Tuple constexpr_tuple{1, 'a', 1.0f};
static_assert(constexpr_tuple.get<0>() == 1);
static_assert(constexpr_tuple.get<1>() == 'a');
static_assert(constexpr_tuple.get<2>() == 1.f);

int main() {
    Tuple tuple2{324, 0xfffffull, "aaaa"};
    assert(tuple2.get<0>() == 324);
    assert(tuple2.get<1>() == 0xfffffull);
}
