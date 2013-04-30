#include "Thread.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include <iostream>
#include <cassert>
#include <SDL_thread.h>

namespace openmsx {

static unsigned mainThreadId = unsigned(-1);

void Thread::setMainThread()
{
	assert(mainThreadId == unsigned(-1));
	mainThreadId = SDL_ThreadID();
}

bool Thread::isMainThread()
{
	assert(mainThreadId != unsigned(-1));
	return mainThreadId == SDL_ThreadID();
}


Thread::Thread(Runnable* runnable_)
	: runnable(runnable_)
	, thread(nullptr)
{
}

Thread::~Thread()
{
	stop();
}

void Thread::start()
{
	assert(!thread);
	thread = SDL_CreateThread(startThread, runnable);
	if (!thread) {
		throw FatalError(StringOp::Builder() <<
			"Unable to create thread: " << SDL_GetError());
    }
}

void Thread::stop()
{
	if (thread) {
		SDL_KillThread(thread);
		thread = nullptr;
	}
}

// TODO: A version with timeout would be useful.
//       After the timeout expires, the method would return false.
//       Alternatively, stop() is called if the thread does not end
//       within the given timeout.
void Thread::join()
{
	if (thread) {
		SDL_WaitThread(thread, nullptr);
		thread = nullptr;
	}
}

int Thread::startThread(void* runnable)
{
	try {
		static_cast<Runnable*>(runnable)->run();
	} catch (FatalError& e) {
		std::cerr << "Fatal error in subthread: "
		     << e.getMessage() << std::endl;
		UNREACHABLE;
	} catch (MSXException& e) {
		std::cerr << "Uncaught exception in subthread: "
		     << e.getMessage() << std::endl;
		UNREACHABLE;
	} // don't catch(..), thread cancelation seems to depend on it
	return 0;
}

} // namespace openmsx
