#include "GlobalCliComm.hh"
#include "CliListener.hh"
#include "Thread.hh"
#include "ScopedAssign.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace openmsx {

GlobalCliComm::GlobalCliComm()
	: sem(1)
	, delivering(false)
{
}

GlobalCliComm::~GlobalCliComm()
{
	assert(Thread::isMainThread());
	assert(!delivering);

	ScopedLock lock(sem);
	// TODO GlobalCliComm has unusual ownership semantics.
	//      Try to rework it.
	for (auto& l : listeners) {
		delete l;
	}
}

void GlobalCliComm::addListener(CliListener* listener)
{
	// can be called from any thread
	ScopedLock lock(sem);
	listeners.push_back(listener);
}

void GlobalCliComm::removeListener(CliListener* listener)
{
	// can be called from any thread
	ScopedLock lock(sem);
	auto it = find(listeners.begin(), listeners.end(), listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

void GlobalCliComm::log(LogLevel level, string_ref message)
{
	assert(Thread::isMainThread());

	if (delivering) {
		// Don't allow recursive calls, this would hang while trying to
		// acquire the Semaphore below. But also when we would change
		// this to a recursive-mutex, this could result in an infinite
		// loop.
		// One example of a recursive invocation is when something goes
		// wrong in the Tcl proc attached to message_callback (e.g. the
		// font used to display the message could not be loaded).
		std::cerr << "Recursive cliComm message: " << message << std::endl;
		return;
	}
	ScopedAssign<bool> sa(delivering, true);

	ScopedLock lock(sem);
	if (!listeners.empty()) {
		for (auto& l : listeners) {
			l->log(level, message);
		}
	} else {
		// don't let the message get lost
		std::cerr << message << std::endl;
	}
}

void GlobalCliComm::update(UpdateType type, string_ref name, string_ref value)
{
	assert(type < NUM_UPDATES);
	auto it = prevValues[type].find(name);
	if (it != prevValues[type].end()) {
		if (it->second == value) {
			return;
		}
		it->second = value.str();
	} else {
		prevValues[type][name] = value.str();
	}
	updateHelper(type, "", name, value);
}

void GlobalCliComm::updateHelper(UpdateType type, string_ref machine,
                                 string_ref name, string_ref value)
{
	assert(Thread::isMainThread());
	ScopedLock lock(sem);
	for (auto& l : listeners) {
		l->update(type, machine, name, value);
	}
}

} // namespace openmsx
