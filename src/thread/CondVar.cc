#include "CondVar.hh"

#include <chrono>

namespace openmsx {

void CondVar::wait()
{
	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock);
}

bool CondVar::waitTimeout(unsigned us)
{
	std::chrono::microseconds duration(us);
	std::unique_lock<std::mutex> lock(mutex);
	return condition.wait_for(lock, duration) == std::cv_status::timeout;
}

void CondVar::signal()
{
	condition.notify_one();
}

void CondVar::signalAll()
{
	condition.notify_all();
}

} // namespace openmsx
