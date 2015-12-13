#ifndef SUBJECT_HH
#define SUBJECT_HH

#include "Observer.hh"
#include "ScopedAssign.hh"
#include "stl.hh"
#include <algorithm>
#include <vector>
#include <cassert>

namespace openmsx {

/**
 * Generic Gang-of-Four Subject class of the Observer pattern, templatized
 * edition.
 */
template <typename T> class Subject
{
public:
	void attach(Observer<T>& observer);
	void detach(Observer<T>& observer);

protected:
	Subject();
	~Subject();
	void notify() const;

private:
	std::vector<Observer<T>*> observers; // unordered
#ifndef NDEBUG
	mutable bool notifyInProgress;
#endif
};

template <typename T> Subject<T>::Subject()
#ifndef NDEBUG
	: notifyInProgress(false)
#endif
{
}

template <typename T> Subject<T>::~Subject()
{
	assert(!notifyInProgress);
	auto copy = observers;
	for (auto& o : copy) {
		o->subjectDeleted(*static_cast<const T*>(this));
	}
	assert(observers.empty());
}

template <typename T> void Subject<T>::attach(Observer<T>& observer)
{
	assert(!notifyInProgress);
	observers.push_back(&observer);
}

template <typename T> void Subject<T>::detach(Observer<T>& observer)
{
	assert(!notifyInProgress);
	move_pop_back(observers, rfind_unguarded(observers, &observer));
}

template <typename T> void Subject<T>::notify() const
{
#ifndef NDEBUG
	assert(!notifyInProgress);
	ScopedAssign<bool> sa(notifyInProgress, true);
#endif

	for (auto& o : observers) {
		o->update(*static_cast<const T*>(this));
	}
}

} // namespace openmsx

#endif
