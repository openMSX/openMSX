// $Id$

#include "GlobalCliComm.hh"
#include "CliListener.hh"
#include <algorithm>
#include <cassert>

using std::map;
using std::string;

namespace openmsx {

GlobalCliComm::GlobalCliComm()
	: sem(1)
{
}

GlobalCliComm::~GlobalCliComm()
{
	ScopedLock lock(sem);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		delete *it;
	}
}

void GlobalCliComm::addListener(CliListener* listener)
{
	ScopedLock lock(sem);
	listeners.push_back(listener);
}

void GlobalCliComm::removeListener(CliListener* listener)
{
	ScopedLock lock(sem);
	Listeners::iterator it = find(listeners.begin(), listeners.end(), listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

void GlobalCliComm::log(LogLevel level, const string& message)
{
	ScopedLock lock(sem);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->log(level, message);
	}
}

void GlobalCliComm::update(UpdateType type, const string& name,
                           const string& value)
{
	assert(type < NUM_UPDATES);
	map<string, string>::iterator it = prevValues[type].find(name);
	if (it != prevValues[type].end()) {
		if (it->second == value) {
			return;
		}
		it->second = value;
	} else {
		prevValues[type][name] = value;
	}
	updateHelper(type, "", name, value);
}

void GlobalCliComm::updateHelper(UpdateType type, const string& machine,
                                 const string& name, const string& value)
{
	ScopedLock lock(sem);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->update(type, machine, name, value);
	}
}

} // namespace openmsx
