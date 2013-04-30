#include "Alarm.hh"
#include "Timer.hh"
#include "Semaphore.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <algorithm>
#include <vector>
#include <cassert>
#include <limits>
#include <SDL.h>

/**
 * The SDL Timer mechanism is not thread safe. (Shortly) after
 * SDL_RemoveTimer() has been called it's still possible that the timer
 * callback is being executed.
 *
 *    http://listas.apesol.org/pipermail/sdl-libsdl.org/2005-May/050136.html
 *
 * To work around this problem we only use one SDL-timer, and make the code
 * robust against spurious timer callback invocations.
 *
 * There is still one possible problem, when AlarmManager gets deleted (openmsx
 * is quit) and we still get a spurious timer callback. But to minimize the
 * chance of this causeing problems, we use a global boolean 'enabled' (see
 * code for details).
 *
 * The code is not optimized for speed, but ATM there are also very few timers
 * needed at the same time, so this shouldn't be a problem.
 */

namespace openmsx {

class AlarmManager
{
public:
	static AlarmManager& instance();
	void registerAlarm(Alarm& alarm);
	void unregisterAlarm(Alarm& alarm);
	void start(Alarm& alarm, unsigned newPeriod);
	void stop(Alarm& alarm);
	bool isPending(const Alarm& alarm);

private:
	AlarmManager();
	~AlarmManager();

	static unsigned timerCallback(unsigned interval, void* param);
	unsigned timerCallback2();

	std::vector<Alarm*> alarms;
	int64_t time;
	SDL_TimerID id;
	Semaphore sem;
	static volatile bool enabled;
};


// to minimize (or fix completely?) the race condition on exit
volatile bool AlarmManager::enabled = false;

AlarmManager::AlarmManager()
	: id(nullptr), sem(1)
{
	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		throw FatalError(StringOp::Builder() <<
			"Couldn't initialize SDL timer subsystem" <<
			SDL_GetError());
	}
	enabled = true;
}

AlarmManager::~AlarmManager()
{
	assert(alarms.empty());
	enabled = false;
	if (id) {
		SDL_RemoveTimer(id);
	}
}

AlarmManager& AlarmManager::instance()
{
	static AlarmManager oneInstance;
	return oneInstance;
}

void AlarmManager::registerAlarm(Alarm& alarm)
{
	ScopedLock lock(sem);
	assert(find(alarms.begin(), alarms.end(), &alarm) == alarms.end());
	alarms.push_back(&alarm);
}

void AlarmManager::unregisterAlarm(Alarm& alarm)
{
	ScopedLock lock(sem);
	auto it = find(alarms.begin(), alarms.end(), &alarm);
	assert(it != alarms.end());
	alarms.erase(it);
}

static int convert(int period)
{
	return std::max(1, period / 1000);
}

void AlarmManager::start(Alarm& alarm, unsigned period)
{
	ScopedLock lock(sem);
	alarm.period = period;
	alarm.time = Timer::getTime() + period;
	alarm.active = true;

	if (id) {
		// there already is a timer
		int64_t diff = time - alarm.time;
		if (diff <= 0) {
			// but we already have an earlier timer, do nothing
		} else {
			// new timer is earlier
			SDL_RemoveTimer(id);
			time = alarm.time;
			id = SDL_AddTimer(convert(period), timerCallback, this);
		}
	} else {
		// no timer yet
		time = alarm.time;
		id = SDL_AddTimer(convert(period), timerCallback, this);
	}
}

void AlarmManager::stop(Alarm& alarm)
{
	ScopedLock lock(sem);
	alarm.active = false;
	// No need to remove timer, we can handle spurious callbacks.
	// Maybe in the future remove it as an optimization?
}

bool AlarmManager::isPending(const Alarm& alarm)
{
	ScopedLock lock(sem);
	return alarm.active;
}

unsigned AlarmManager::timerCallback(unsigned /*interval*/, void* param)
{
	// note: runs in a different thread!
	if (!enabled) return 0;
	auto manager = static_cast<AlarmManager*>(param);
	return manager->timerCallback2();
}

unsigned AlarmManager::timerCallback2()
{
	// note: runs in a different thread!
	ScopedLock lock(sem);

	int64_t now = Timer::getTime();
	int64_t earliest = std::numeric_limits<int64_t>::max();
	for (auto& a : alarms) {
		if (a->active) {
			// timer active
			// note: don't compare time directly (a->time < now),
			//       because there is a small chance time will wrap
			int64_t left = a->time - now;
			if (left <= 0) {
				// timer expired
				if (a->alarm()) {
					// repeat
					a->time += a->period;
					left = a->time - now;
					// 'left' can still be negative at this
					// point, but that's ok .. convert()
					// will return '1' in that case
					earliest = std::min(earliest, left);
				} else {
					a->active = false;
				}
			} else {
				// timer active but not yet expired
				earliest = std::min(earliest, left);
			}
		}
	}
	if (earliest != std::numeric_limits<int64_t>::max()) {
		time = earliest + now;
		assert(id);
		return convert(int(earliest));
	} else {
		for (auto& a : alarms) {
			assert(a->active == false); (void)a;
		}
		id = nullptr;
		return 0; // don't repeat
	}
}


// class Alarm

Alarm::Alarm()
	: manager(AlarmManager::instance())
	, active(false)
	, destructing(false)
{
	manager.registerAlarm(*this);
}

Alarm::~Alarm()
{
	assert(destructing); // prepareDelete() must be called in subclass
}

void Alarm::prepareDelete()
{
	assert(!destructing); // not yet called
	manager.unregisterAlarm(*this);
	destructing = true;
}

void Alarm::schedule(unsigned newPeriod)
{
	manager.start(*this, newPeriod);
}

void Alarm::cancel()
{
	manager.stop(*this);
}

bool Alarm::pending() const
{
	return manager.isPending(*this);
}

} // namespace openmsx
