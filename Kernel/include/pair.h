#pragma once

template <typename T1, typename T2>
class Pair{
public:
    T1 item1;
    T2 item2;

    Pair(T1 one, T2 two){
        item1 = one;
        item2 = two;
    }
};