#include "Thread.hh"
#include <cassert>
#include <thread>

namespace openmsx::Thread {

static std::jthread::id mainThreadId;

void setMainThread()
{
	assert(mainThreadId == std::jthread::id());
	mainThreadId = std::this_thread::get_id();
}

bool isMainThread()
{
	assert(mainThreadId != std::jthread::id());
	return mainThreadId == std::this_thread::get_id();
}

} // namespace openmsx::Thread
