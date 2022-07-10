#include "GlobalCliComm.hh"
#include "CliListener.hh"
#include "CliConnection.hh"
#include "Thread.hh"
#include "ScopedAssign.hh"
#include "stl.hh"
#include <cassert>
#include <iostream>
#include <utility>

namespace openmsx {

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
	auto it = rfind_unguarded(listeners, &listener,
		[](const auto& ptr) { return ptr.get(); });
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

void GlobalCliComm::log(LogLevel level, std::string_view message)
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
		std::cerr << "Recursive cliComm message: " << message << '\n';
		return;
	}
	ScopedAssign sa(delivering, true);

	std::lock_guard<std::mutex> lock(mutex);
	if (!listeners.empty()) {
		for (auto& l : listeners) {
			l->log(level, message);
		}
	} else {
		// don't let the message get lost
		std::cerr << message << '\n';
	}
}

void GlobalCliComm::update(UpdateType type, std::string_view name, std::string_view value)
{
	assert(type < NUM_UPDATES);
	updateHelper(type, {}, name, value);
}

void GlobalCliComm::updateFiltered(UpdateType type, std::string_view name, std::string_view value)
{
	assert(type < NUM_UPDATES);
	if (auto [it, inserted] = prevValues[type].try_emplace(name, value);
	    !inserted) { // was already present ..
		if (it->second == value) {
			return; // .. with the same value
		} else {
			it->second = value; // .. but with a different value
		}
	}
	updateHelper(type, {}, name, value);
}

void GlobalCliComm::updateHelper(UpdateType type, std::string_view machine,
                                 std::string_view name, std::string_view value)
{
	assert(Thread::isMainThread());
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& l : listeners) {
		l->update(type, machine, name, value);
	}
}

} // namespace openmsx
