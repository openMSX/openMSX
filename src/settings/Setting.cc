// $Id$

#include <cassert>
#include "Setting.hh"
#include "SettingListener.hh"
#include "CliCommOutput.hh"

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
	CliCommOutput::instance().update(CliCommOutput::SETTING, getName(),
	                                 getValueString());
}

void Setting::addListener(SettingListener* listener)
{
	listeners.push_back(listener);
}

void Setting::removeListener(SettingListener* listener)
{
	assert(count(listeners.begin(), listeners.end(), listener) == 1);
	listeners.erase(find(listeners.begin(), listeners.end(), listener));
}

} // namespace openmsx
