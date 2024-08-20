#include "../include/le/list.h"

#include <assert.h>

int main() {
    List<int> list;

    list.add_back(1);
    list.add_back(2);
    list.add_back(3);

    int i = 1;
    for (const auto &item : list) {
        assert(item == i);

        i++;
    }

    list.remove_at(1);

    assert(list[0] == 1);
    assert(list[1] == 3);

    list.remove_at(0);

    assert(list[0] == 3);
    assert(list.get_length() == 1);

    return 0;
}
