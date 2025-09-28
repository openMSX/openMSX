#include "GlobalCliComm.hh"

#include "CliConnection.hh"
#include "CliListener.hh"

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

CliListener* GlobalCliComm::addListener(std::unique_ptr<CliListener> listener)
{
	// can be called from any thread
	std::scoped_lock lock(mutex);
	auto* p = listener.get();
	listeners.push_back(std::move(listener));
	if (allowExternalCommands) {
		if (auto* conn = dynamic_cast<CliConnection*>(p)) {
			conn->start();
		}
	}
	return p;
}

std::unique_ptr<CliListener> GlobalCliComm::removeListener(CliListener& listener)
{
	// can be called from any thread
	std::scoped_lock lock(mutex);
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
	for (const auto& listener : listeners) {
		if (auto* conn = dynamic_cast<CliConnection*>(listener.get())) {
			conn->start();
		}
	}
}

void GlobalCliComm::log(LogLevel level, std::string_view message, float fraction)
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

	std::scoped_lock lock(mutex);
	if (!listeners.empty()) {
		for (const auto& l : listeners) {
			l->log(level, message, fraction);
		}
	} else {
		// don't let the message get lost
		std::cerr << message;
		if (level == LogLevel::PROGRESS && fraction >= 0.0f) {
			std::cerr << "... " << int(100.0f * fraction) << '%';
		}
		std::cerr << '\n';
	}
}

void GlobalCliComm::update(UpdateType type, std::string_view name, std::string_view value)
{
	updateHelper(type, {}, name, value);
}

void GlobalCliComm::updateFiltered(UpdateType type, std::string_view name, std::string_view value)
{
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
	std::scoped_lock lock(mutex);
	for (const auto& l : listeners) {
		l->update(type, machine, name, value);
	}
}

} // namespace openmsx
