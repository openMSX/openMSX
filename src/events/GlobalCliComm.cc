#include "GlobalCliComm.hh"
#include "CliListener.hh"
#include "CliConnection.hh"
#include "Thread.hh"
#include "ScopedAssign.hh"
#include "stl.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace openmsx {

GlobalCliComm::GlobalCliComm()
	: delivering(false)
	, allowExternalCommands(false)
{
}

GlobalCliComm::~GlobalCliComm()
{
	assert(Thread::isMainThread());
	assert(!delivering);
}

void GlobalCliComm::addListener(std::unique_ptr<CliListener> listener)
{
	// can be called from any thread
	std::lock_guard<std::mutex> lock(mutex);
	auto* p = listener.get();
	listeners.push_back(std::move(listener));
	if (allowExternalCommands) {
		if (auto* conn = dynamic_cast<CliConnection*>(p)) {
			conn->start();
		}
	}
}

std::unique_ptr<CliListener> GlobalCliComm::removeListener(CliListener& listener)
{
	// can be called from any thread
	std::lock_guard<std::mutex> lock(mutex);
	auto it = rfind_if_unguarded(listeners,
		[&](const std::unique_ptr<CliListener>& ptr) {
			return ptr.get() == &listener; });
	auto result = std::move(*it);
	move_pop_back(listeners, it);
	return result;
}

void GlobalCliComm::setAllowExternalCommands()
{
	assert(!allowExternalCommands); // should only be called once
	allowExternalCommands = true;
	for (auto& listener : listeners) {
		if (auto* conn = dynamic_cast<CliConnection*>(listener.get())) {
			conn->start();
		}
	}
}

void GlobalCliComm::log(LogLevel level, string_ref message)
{
	assert(Thread::isMainThread());

	if (delivering) {
		// Don't allow recursive calls, this would hang while trying to
		// acquire the mutex below. But also when we would change
		// this to a recursive-mutex, this could result in an infinite
		// loop.
		// One example of a recursive invocation is when something goes
		// wrong in the Tcl proc attached to message_callback (e.g. the
		// font used to display the message could not be loaded).
		std::cerr << "Recursive cliComm message: " << message << std::endl;
		return;
	}
	ScopedAssign<bool> sa(delivering, true);

	std::lock_guard<std::mutex> lock(mutex);
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
	if (it != end(prevValues[type])) {
		if (it->second == value) {
			return;
		}
		it->second = value.str();
	} else {
		prevValues[type].emplace_noDuplicateCheck(name.str(), value.str());
	}
	updateHelper(type, "", name, value);
}

void GlobalCliComm::updateHelper(UpdateType type, string_ref machine,
                                 string_ref name, string_ref value)
{
	assert(Thread::isMainThread());
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& l : listeners) {
		l->update(type, machine, name, value);
	}
}

} // namespace openmsx
