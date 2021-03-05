#pragma once

#include <Lemon/Types.h>

#include <vector>
#include <list>

namespace Lemon{
    class Waitable {
        friend class Waiter;
    protected:
        std::list<class Waiter*> waiters;
    public:
        virtual inline const handle_t& GetHandle() const = 0;
        virtual inline void GetAllHandles(std::vector<handle_t>& v) const { v.push_back(GetHandle()); };
        void Wait(long timeout = -1);

        virtual ~Waitable();
    };

    class Waiter {
        std::list<Waitable*> waitingOn;
        std::list<Waitable*> waitingOnAll;
        std::vector<handle_t> handles;
    public:
        void RepopulateHandles();

        /////////////////////////////
        /// \brief Interface::WaitOn(waitable) - Wait on an object
        ///
        /// Add a waitable to the list
        ///
        /// \param waitable Class extending waitable
        /////////////////////////////
        void WaitOn(Waitable* waitable);

        /////////////////////////////
        /// \brief Interface::WaitOn(waitable) - Wait on many objects
        ///
        /// Add a waitable containing many handles to the list
        ///
        /// \param waitable Class extending waitable
        /////////////////////////////
        void WaitOnAll(Waitable* waitable);

        void StopWaitingOn(Waitable* waitable);
        void StopWaitingOnAll(Waitable* waitable);

        void Wait(long timeout = -1);

        virtual ~Waiter();
    };
}