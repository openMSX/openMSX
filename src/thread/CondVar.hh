// $Id$

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
	void wait();
	int waitTimeout(Uint32);
	void signal();
	void signalAll();

private:
	SDL_cond* cond;
	SDL_mutex* mutex;
};

} // namespace openmsx

#endif
