// $Id: $

#ifndef CONDVAR_HH
#define CONDVAR_HH

struct SDL_cond;
struct SDL_mutex;

namespace openmsx {

class CondVar
{
public:
	CondVar();
	~CondVar();

	/** Block till another thread signals this condition variable.
	  */
	void wait();

	/** Same as wait(), but with a timeout.
	  * @param The maximum time to wait, in micro seconds.
	  * @result Returns true iff we return because of a timeout.
	  */
	bool waitTimeout(unsigned us);

	/** Wake on thread that's waiting on this condtition variable.
	 */
	void signal();

	/** Wake all threads that are waiting on this condtion variable.
	 */
	void signalAll();

private:
	SDL_cond* cond;
	SDL_mutex* mutex;
};

} // namespace openmsx

#endif
