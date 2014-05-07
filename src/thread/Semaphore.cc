#include "Semaphore.hh"

namespace openmsx {

Semaphore::Semaphore(unsigned value)
	: value(value)
{
}

void Semaphore::up()
{
	std::unique_lock<std::mutex> lock(mutex);
	value++;
	condition.notify_one();
}

void Semaphore::down()
{
	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock, [&]() { return value != 0; });
	value--;
}

} // namespace openmsx
