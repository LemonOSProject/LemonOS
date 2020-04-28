#ifndef _LIBALLOC_H
#define _LIBALLOC_H

#include <stdint.h>
#include <stddef.h>

//#define malloc kmalloc
//#define free kfree
//#define realloc krealloc

/** \defgroup ALLOCHOOKS liballoc hooks
*
* These are the OS specific functions which need to
* be implemented on any platform that the library
* is expected to work on.
*/

/** @{ */



// If we are told to not define our own size_t, then we skip the define.
//#define _HAVE_UINTPTR_T
//typedef	unsigned long	uintptr_t;

//This lets you prefix malloc and friends
#define PREFIX(func)		k ## func

#ifdef __cplusplus
extern "C" {
#endif



	/** This function is supposed to lock the memory data structures. It
	* could be as simple as disabling interrupts or acquiring a spinlock.
	* It's up to you to decide.
	*
	* \return 0 if the lock was acquired successfully. Anything else is
	* failure.
	*/
	extern int liballoc_lock();

	/** This function unlocks what was previously locked by the liballoc_lock
	* function.  If it disabled interrupts, it enables interrupts. If it
	* had acquiried a spinlock, it releases the spinlock. etc.
	*
	* \return 0 if the lock was successfully released.
	*/
	extern int liballoc_unlock();

	/** This is the hook into the local system which allocates pages. It
	* accepts an integer parameter which is the number of pages
	* required.  The page size was set up in the liballoc_init function.
	*
	* \return NULL if the pages were not allocated.
	* \return A pointer to the allocated memory.
	*/
	extern void* liballoc_alloc(size_t);

	/** This frees previously allocated memory. The void* parameter passed
	* to the function is the exact same value returned from a previous
	* liballoc_alloc call.
	*
	* The integer value is the number of pages to free.
	*
	* \return 0 if the memory was successfully freed.
	*/
	extern int liballoc_free(void*, size_t);

	extern int templiballoc_unlock();

#if defined DEBUG || defined INFO || true
	void liballoc_dump();
#endif


	extern void    *PREFIX(malloc)(size_t);				///< The standard function.
	extern void    *PREFIX(realloc)(void *, size_t);		///< The standard function.
	extern void    *PREFIX(calloc)(size_t, size_t);		///< The standard function.
	extern void     PREFIX(free)(void *);					///< The standard function.
	extern void *PREFIX(tempmalloc)(size_t req_size);

#ifdef __cplusplus
}
#endif

/** @} */

#endif


