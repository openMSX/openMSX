// $Id$

#include "Setting.hh"
#include "SettingListener.hh"
#include "CliComm.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

Setting::Setting(const string& name_, const string& description_)
	: name(name_), description(description_)
{
}

Setting::~Setting()
{
	assert(listeners.empty());
}

void Setting::notify() const
{
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->update(this);
	}
	CliComm::instance().update(CliComm::SETTING, getName(),
	                           getValueString());
}

void Setting::addListener(SettingListener* listener)
{
	listeners.push_back(listener);
}

void Setting::removeListener(SettingListener* listener)
{
	assert(std::count(listeners.begin(), listeners.end(), listener) == 1);
	listeners.erase(std::find(listeners.begin(), listeners.end(), listener));
}

} // namespace openmsx
