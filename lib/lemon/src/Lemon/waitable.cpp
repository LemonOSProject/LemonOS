#include <Lemon/System/Waitable.h>

#include <Lemon/System/KernelObject.h>

namespace Lemon {
void Waitable::Wait(long timeout) { WaitForKernelObject(GetHandle(), timeout); }

Waitable::~Waitable() {
    for (Waiter* waiter : waiters) {
        waiter->StopWaitingOn(this);
    }
}

void Waiter::WaitOn(Waitable* waitable) {
    waitingOn.push_back(waitable);
    handles.push_back(waitable->GetHandle());

    waitable->waiters.push_back(this);
}

void Waiter::WaitOnAll(Waitable* waitable) {
    waitingOnAll.push_back(waitable);

    waitable->GetAllHandles(handles); // Get all handles

    waitable->waiters.push_back(this);
}

void Waiter::StopWaitingOn(Waitable* waitable) {
    waitable->waiters.remove(this);
    waitingOn.remove(waitable);

    RepopulateHandles();
}

void Waiter::StopWaitingOnAll(Waitable* waitable) {
    waitable->waiters.remove(this);
    waitingOnAll.remove(waitable);

    RepopulateHandles();
}

void Waiter::Wait(long timeout) {
    if (handles.size()) {
        WaitForKernelObject(handles.data(), handles.size(), timeout);
    }
}

void Waiter::RepopulateHandles() {
    handles.clear();

    for (auto& waitable : waitingOn) {
        handles.push_back(waitable->GetHandle());
    }

    for (auto& waitable : waitingOnAll) {
        waitable->GetAllHandles(handles);
    }
}

Waiter::~Waiter() {
    for (auto& waitable : waitingOn) {
        waitable->waiters.remove(this);
    }

    for (auto& waitable : waitingOnAll) {
        waitable->waiters.remove(this);
    }
}
} // namespace Lemon
