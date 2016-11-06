#include "Thread.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "unreachable.hh"
#include <iostream>
#include <cassert>
#include <system_error>

namespace openmsx {

static std::thread::id mainThreadId;

void Thread::setMainThread()
{
	assert(mainThreadId == std::thread::id());
	mainThreadId = std::this_thread::get_id();
}

bool Thread::isMainThread()
{
	assert(mainThreadId != std::thread::id());
	return mainThreadId == std::this_thread::get_id();
}


Thread::Thread(Runnable* runnable_)
	: runnable(runnable_)
{
}

Thread::~Thread()
{
	// Our join() resets the thread pointer, so if the pointer is non-null,
	// we have a running thread. If we do nothing, std::thread will terminate
	// the process, but this assert is easier to debug.
	assert(!thread);
}

void Thread::start()
{
	assert(!thread);
	try {
		thread = make_unique<std::thread>(startThread, runnable);
	} catch (const std::system_error& e) {
		throw FatalError(StringOp::Builder() <<
			"Unable to create thread: " << e.what());
    }
}

// TODO: A version with timeout would be useful.
//       After the timeout expires, the method would return false.
void Thread::join()
{
	if (thread) {
		thread->join();
		thread.reset();
	}
}

int Thread::startThread(Runnable* runnable)
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
