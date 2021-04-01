#include <Thread.h>

#include <Scheduler.h>
#include <CPU.h>
#include <Debug.h>
#include <Timer.h>
#include <TimerEvent.h>

#ifdef KERNEL_DEBUG
	#include <StackTrace.h>
#endif

void ThreadBlocker::Interrupt(){
	interrupted = true;
	shouldBlock = false;

	acquireLock(&lock);
	if(thread){
		thread->Unblock();
	}
	releaseLock(&lock);
}

void ThreadBlocker::Unblock() {
	shouldBlock = false;
    removed = true;

	acquireLock(&lock);
	if(thread){
		thread->Unblock();
	}
	releaseLock(&lock);
}

bool Thread::Block(ThreadBlocker* newBlocker){
	assert(CheckInterrupts());

	acquireLock(&newBlocker->lock);

	asm("cli");
	newBlocker->thread = this;
	if(!newBlocker->ShouldBlock()){
		releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocked
		asm("sti");

		return false;
	}

	blocker = newBlocker;

	releaseLock(&newBlocker->lock);
	state = ThreadStateBlocked;
	asm("sti");

	Scheduler::Yield();

	blocker = nullptr;

	return newBlocker->WasInterrupted();
}

bool Thread::Block(ThreadBlocker* newBlocker, long& usTimeout){
	assert(CheckInterrupts());
	
	Timer::TimerCallback timerCallback = [](void* t) -> void {
		reinterpret_cast<Thread*>(t)->blockTimedOut = true;
		reinterpret_cast<Thread*>(t)->Unblock();
	};

	acquireLock(&newBlocker->lock);
	if(!newBlocker->ShouldBlock()){
		releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocks

		return false;
	}

	blockTimedOut = false;
	blocker = newBlocker;

	blocker->thread = this;

	{
		Timer::TimerEvent ev(usTimeout, timerCallback, this);
		
		asm("cli");
		releaseLock(&newBlocker->lock);
		state = ThreadStateBlocked;

		// Now that the thread state has been set blocked, check if we timed out before setting to blocked
		if(!blockTimedOut) {
			asm("sti");

			Scheduler::Yield();
		} else {
			state = ThreadStateRunning;

			asm("sti");

			blocker->Interrupt(); // If the blocker re-calls Thread::Unblock that's ok
		}
	}

	if(blockTimedOut){
		blocker->Interrupt();
		usTimeout = 0;
	}

	return (!blockTimedOut) && newBlocker->WasInterrupted();
}

void Thread::Sleep(long us){
	assert(CheckInterrupts());

	blockTimedOut = false;
	
	Timer::TimerCallback timerCallback = [](void* t) -> void {
		reinterpret_cast<Thread*>(t)->blockTimedOut = true;
		reinterpret_cast<Thread*>(t)->Unblock(); 
	};
	
	{
		Timer::TimerEvent ev(us, timerCallback, this);
				
		asm("cli");
		state = ThreadStateBlocked;

		// Now that the thread state has been set blocked, check if we timed out before setting to blocked
		if(!blockTimedOut) {
			asm("sti");

			Scheduler::Yield();
		} else {
			state = ThreadStateRunning;

			asm("sti");
		}
	}
}

void Thread::Unblock(){
	timeSlice = timeSliceDefault;

	if(state != ThreadStateZombie)
		state = ThreadStateRunning;
}