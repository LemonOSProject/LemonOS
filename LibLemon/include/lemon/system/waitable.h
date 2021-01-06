#pragma once

#include <lemon/types.h>

#include <list>

namespace Lemon{
    class Waitable {
        std::list<class Waiter> waiters;

    public:
        virtual inline const handle_t& GetHandle() const = 0;
        void Wait();

        virtual ~Waitable() = default;
    };

    class Waiter {
        void WaitOn(Waitable& waitable);
        void Wait();
    };
}