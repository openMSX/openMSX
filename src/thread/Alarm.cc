// $Id$

#include "Alarm.hh"
#include <SDL.h>
#include <cassert>

namespace openmsx {

/**
 * The SDL Timer mechanism is not thread safe. (Shortly) after
 * SDL_RemoveTimer() has been called it's still possible that the timer
 * callback is being executed.
 *
 * The most likely scenario of this problem is when the callback is being
 * executed in the timer thread while another thread executes the
 * Alarm::cancel() method. Even after cancel() returns the timer thread will
 * still be executing the callback. Ideally cancel() only returns when the
 * callback is finished.
 *
 * The code below tries to minimize the chance that this race occures, but
 * AFAICS, without a better SDL implementation, it's not possible to completely
 * eliminate it. So, to be safe write code so that callback can be called even
 * after it has been canceled. A problem that is more difficult to solve is
 * deletion of an Alarm object, so try to reuse Alarm objects as much as
 * possible.
 */

Alarm::Alarm()
	: id(NULL), sem(1)
{
}

Alarm::~Alarm()
{
	assert(!pending());
}

void Alarm::schedule(unsigned interval)
{
	sem.down();
	do_cancel();
	id = SDL_AddTimer(interval / 1000, helper, this);
	sem.up();
}

void Alarm::cancel()
{
	sem.down();
	do_cancel();
	sem.up();
}

void Alarm::do_cancel()
{
	if (id) {
		SDL_RemoveTimer(static_cast<SDL_TimerID>(id));
		id = 0;
	}
}

bool Alarm::pending() const
{
	sem.down();
	bool result = id;
	sem.up();
	return result;
}

unsigned Alarm::helper(unsigned /*interval*/, void* param)
{
	// At this position there is a race condition:
	//   if the timer thread is suspended before the lock is taken and
	//   another thread cancels and deletes this Alarm object, we have a
	//   free memory read when the timer thread continues.
	// TODO If the other thread just cancels the alarm there is no problem.
	//      (Is this correct???) TODO adjust the documentation at the top
	
	Alarm* alarm = static_cast<Alarm*>(param);
	alarm->sem.down();
	if (alarm->id) {
		alarm->alarm();
		alarm->id = 0;
	}
	alarm->sem.up();
	return 0;
}

} // namespace openmsx
