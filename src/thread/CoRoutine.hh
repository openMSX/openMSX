
#ifndef __COROUTINE_HH__
#define __COROUTINE_HH__

// select coroutine library
#ifdef _WIN32
#define USE_WIN32_FIBERS
#else
#define USE_PCL
#endif

#ifdef USE_PCL
#include "pcl.hh"
#endif

#ifdef USE_WIN32_FIBERS
// Fiber API is not accessible in MSVC6.0 without a hack (next 3 lines):
#if _MSC_VER==1200
#define _WIN32_WINNT 0x0400 
#endif 
#include <windows.h>
#endif

class CoRoutine
{
public:
	static void call(CoRoutine& coRoutine);
	static void resume();
	//static void exitTo(CoRoutine& coRoutine);
	//static void exit();

protected:
	explicit CoRoutine(unsigned stackSize);
	virtual ~CoRoutine();

	virtual void run() = 0;

private:
	static void trampoline(void* coRoutine);
	
#ifdef USE_PCL
	coroutine* cor;
#endif

#ifdef USE_WIN32_FIBERS
	void init();
	
	LPVOID lpFiber;
	LPVOID caller;
	static LPVOID currentFiber;
#endif

};

#endif
