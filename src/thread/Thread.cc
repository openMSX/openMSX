// $Id$

#include "Thread.hh"
#include "MSXException.hh"
#include <iostream>
#include <cstddef>
#include <cassert>
#include <SDL_thread.h>

using std::cerr;
using std::endl;

namespace openmsx {

Thread::Thread(Runnable* runnable_)
	: runnable(runnable_)
{
	thread = NULL;
}

Thread::~Thread()
{
	stop();
}

void Thread::start()
{
	assert(!thread);
	thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	if (thread) {
		SDL_KillThread(thread);
		thread = NULL;
	}
}

// TODO: A version with timeout would be useful.
//       After the timeout expires, the method would return false.
//       Alternatively, stop() is called if the thread does not end
//       within the given timeout.
void Thread::join()
{
	assert(thread);
	SDL_WaitThread(thread, NULL);
	thread = NULL;
}

int Thread::startThread(void* runnable)
{
	try {
		static_cast<Runnable*>(runnable)->run();
	} catch (FatalError& e) {
		cerr << "Fatal error in subthread: "
		     << e.getMessage() << endl;
		assert(false);
	} catch (MSXException& e) {
		cerr << "Uncaught exception in subthread: "
		     << e.getMessage() << endl;
		assert(false);
	} // don't catch(..), thread cancelation seems to depend on it
	return 0;
}

} // namespace openmsx
