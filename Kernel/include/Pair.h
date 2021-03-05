#pragma once

template <typename T1, typename T2>
class Pair{
public:
    T1 item1;
    T2 item2;

    Pair(const T1 one, const T2 two){
        item1 = one;
        item2 = two;
    }

    Pair(){

    }
};