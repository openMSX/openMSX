// $Id$

#include "Setting.hh"
#include "SettingListener.hh"
#include "CliComm.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

Setting::Setting(const string& name_, const string& description_, SaveSetting save_)
	: name(name_), description(description_), notifyInProgress(0)
	, save(save_ == SAVE)
{
}

Setting::~Setting()
{
	assert(listeners.empty());
}

void Setting::notify() const
{
	++notifyInProgress;
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->update(this);
	}
	CliComm::instance().update(CliComm::SETTING, getName(),
	                           getValueString());
	--notifyInProgress;
}

void Setting::addListener(SettingListener* listener)
{
	assert(!notifyInProgress);
	listeners.push_back(listener);
}

void Setting::removeListener(SettingListener* listener)
{
	assert(!notifyInProgress);
	assert(std::count(listeners.begin(), listeners.end(), listener) == 1);
	listeners.erase(std::find(listeners.begin(), listeners.end(), listener));
}

bool Setting::needLoadSave() const
{
	return save;
}

} // namespace openmsx
