#include <cassert>
#include "CoRoutine.hh"


#ifdef USE_PCL

CoRoutine::CoRoutine(unsigned stackSize)
{
	cor = co_create(trampoline, this, 0, stackSize);
}

CoRoutine::~CoRoutine()
{
	co_delete(cor);
}

void CoRoutine::call(CoRoutine& other)
{
	co_call(other.cor);
}

void CoRoutine::resume()
{
	co_resume();
}

/*
void CoRoutine::exitTo(CoRoutine& other)
{
	co_exit_to(other.cor);
}

void CoRoutine::exit()
{
	co_exit();
}
*/

#endif

#ifdef USE_WIN32_FIBERS

LPVOID CoRoutine::currentFiber;

CoRoutine::CoRoutine(unsigned stackSize)
	: caller(0)
{
	init();
	
	lpFiber = CreateFiber(stackSize, trampoline, this);
	assert(lpFiber);
}

void cCoroutine::init()
{
	static bool alreadyInit = false;
	if (alreadyInit) {
		return;
	}
	alreadyInit = true;

	currentFiber = ConvertThreadToFiber(0);
}

CoRoutine::~CoRoutine()
{
	assert(currentFiber != lpFiber);
	DeleteFiber(lpFiber);
}

void CoRoutine::call(CoRoutine& coRoutine)
{
	assert(!coRoutine.caller);
	coRoutine.caller = currentFiber;
	currentFiber = coRoutine.lpFiber;
	SwitchToFiber(currentFiber);
}

void CoRoutine::resume()
{
	assert(caller);
	LPVOID oldCaller = caller;
	caller = 0;
	SwitchToFiber(oldCaller);
}

#endif

void CoRoutine::trampoline(void* coRoutine)
{
	static_cast<CoRoutine*>(coRoutine)->run();
}


