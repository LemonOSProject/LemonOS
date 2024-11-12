#include "../include/le/pq.h"

#include <assert.h>

int main() {
    PQ<int, int> pq;

    int r = pq.push(50, 5)
     || pq.push(60, 3)
     || pq.push(70, 4)
     || pq.push(80, 2)
     || pq.push(90, 20)
     || pq.push(100, 0);

    assert(!r);

    assert(pq.pop() == 100);
    assert(pq.pop() == 80);
    assert(pq.pop() == 60);

    r = pq.push(-1337, 2);
    assert(!r);

    assert(pq.pop() == -1337);
    assert(pq.pop() == 70);

    r = pq.push(-1337, 25);

    assert(pq.pop() == 50);
    assert(pq.pop() == 90);
    assert(pq.pop() == -1337);

    return 0;
}
