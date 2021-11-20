#ifndef SUBJECT_HH
#define SUBJECT_HH

#include "Observer.hh"
#include "ranges.hh"
#include "stl.hh"
#include <algorithm>
#include <vector>
#include <cassert>

namespace openmsx {

/**
 * Generic Gang-of-Four Subject class of the Observer pattern, templatized
 * edition.
 */
template<typename T> class Subject
{
public:
	void attach(Observer<T>& observer);
	void detach(Observer<T>& observer);
	[[nodiscard]] bool anyObservers() const { return !observers.empty(); }

protected:
	Subject() = default;
	~Subject();
	void notify() const;

private:
	enum NotifyState {
		IDLE,        // no notify in progress
		IN_PROGRESS, // notify in progress, no detach
		DETACH,      // notify in progress, some observer(s) have been detached
	};

	mutable std::vector<Observer<T>*> observers; // unordered
	mutable NotifyState notifyState = IDLE;
};

template<typename T> Subject<T>::~Subject()
{
	assert(notifyState == IDLE);
	auto copy = observers;
	for (auto& o : copy) {
		o->subjectDeleted(*static_cast<const T*>(this));
	}
	assert(observers.empty());
}

template<typename T> void Subject<T>::attach(Observer<T>& observer)
{
	assert(notifyState == IDLE);
	observers.push_back(&observer);
}

template<typename T> void Subject<T>::detach(Observer<T>& observer)
{
	auto it = rfind_unguarded(observers, &observer);
	if (notifyState == IDLE) {
		move_pop_back(observers, it);
	} else {
		*it = nullptr; // mark for removal
		notifyState = DETACH; // schedule actual removal pass
	}
}

template<typename T> void Subject<T>::notify() const
{
	assert(notifyState == IDLE);
	notifyState = IN_PROGRESS;

	for (auto& o : observers) {
		try {
			o->update(*static_cast<const T*>(this));
		} catch (...) {
			assert(false && "Observer::update() shouldn't throw");
		}
	}

	if (notifyState == DETACH) {
		observers.erase(ranges::remove(observers, nullptr), observers.end());
	}
	notifyState = IDLE;
}

} // namespace openmsx

#endif
