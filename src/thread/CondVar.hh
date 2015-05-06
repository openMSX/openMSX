#ifndef CONDVAR_HH
#define CONDVAR_HH

#include <condition_variable>
#include <mutex>

namespace openmsx {

class CondVar
{
public:
	/** Block till another thread signals this condition variable.
	  */
	void wait();

	/** Same as wait(), but with a timeout.
	  * @param us The maximum time to wait, in micro seconds.
	  * @result Returns true if we return because of a timeout.
	  */
	bool waitTimeout(unsigned us);

	/** Wake on thread that's waiting on this condtition variable.
	 */
	void signal();

	/** Wake all threads that are waiting on this condition variable.
	 */
	void signalAll();

private:
	std::mutex mutex;
	std::condition_variable condition;
};

} // namespace openmsx

#endif
