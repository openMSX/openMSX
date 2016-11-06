#include "Thread.hh"
#include "MSXException.hh"
#include "StringOp.hh"
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
	// Thread must be join()ed before it's destructed. If not
	// std::terminate() will be called, but this assert might be easier to
	// debug.
	assert(!thread.joinable());
}

void Thread::start()
{
	assert(!thread.joinable());
	try {
		thread = std::thread(startThread, runnable);
	} catch (const std::system_error& e) {
		throw FatalError(StringOp::Builder() <<
			"Unable to create thread: " << e.what());
    }
}

void Thread::join()
{
	thread.join();
}

void Thread::startThread(Runnable* runnable)
{
	try {
		runnable->run();
	} catch (FatalError& e) {
		std::cerr << "Fatal error in subthread: "
		     << e.getMessage() << std::endl;
	} catch (MSXException& e) {
		std::cerr << "Uncaught exception in subthread: "
		     << e.getMessage() << std::endl;
	} catch (...) {
		// If any exception escapes std::terminate() is called. Possibly
		// this extra print helps to locate the problem.
		std::cerr << "unknown exception in subthread"
		     << std::endl;
		throw; // calls std::terminate()
	}
}

} // namespace openmsx
