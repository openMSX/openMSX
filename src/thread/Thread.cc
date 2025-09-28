#include "Thread.hh"

#include <cassert>
#include <thread>

namespace openmsx::Thread {

static std::thread::id mainThreadId;

void setMainThread()
{
	assert(mainThreadId == std::thread::id());
	mainThreadId = std::this_thread::get_id();
}

bool isMainThread()
{
	assert(mainThreadId != std::thread::id());
	return mainThreadId == std::this_thread::get_id();
}

} // namespace openmsx::Thread
