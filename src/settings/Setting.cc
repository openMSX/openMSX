// $Id$

#include "Setting.hh"
#include "SettingListener.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "XMLElement.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

Setting::Setting(CommandController& commandController_, const string& name_,
                 const string& description_, SaveSetting save_)
	: commandController(commandController_), name(name_)
	, description(description_), notifyInProgress(0)
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
	commandController.getCliComm().update(CliComm::SETTING, getName(),
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

void Setting::sync(XMLElement& config) const
{
	XMLElement& settings = config.getCreateChild("settings");
	if (hasDefaultValue()) {
		// remove setting
		const XMLElement* elem = settings.findChildWithAttribute(
				"setting", "id", getName());
		if (elem) settings.removeChild(*elem);
	} else {
		// add (or overwrite) setting
		XMLElement& elem = settings.getCreateChildWithAttribute(
				"setting", "id", getName());
		elem.setData(getValueString());
	}
}

CommandController& Setting::getCommandController() const
{
	return commandController;
}

} // namespace openmsx
