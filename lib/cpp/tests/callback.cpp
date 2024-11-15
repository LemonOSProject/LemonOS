#include "../include/le/callback.h"

#include <assert.h>

int main() {
    int x = 0;
    int y = 0;

    auto fn = [&x, &y]() {
        x = 5;
        y = 10;
    };

    auto cb = Callback<>::create(&fn);

    cb.fn(cb.data);

    assert(x == 5);
    assert(y == 10);

    return 0;
}
