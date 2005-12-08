// $Id$

#include "Setting.hh"
#include "Observer.hh"
#include "CommandController.hh"
#include "TclObject.hh"
#include "CliComm.hh"
#include "XMLElement.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

Setting::Setting(CommandController& commandController_, const string& name_,
                 const string& description_, SaveSetting save_)
	: commandController(commandController_), name(name_)
	, description(description_), save(save_ == SAVE)
{
}

Setting::~Setting()
{
}

void Setting::notify() const
{
	Subject<Setting>::notify();
	commandController.getCliComm().update(CliComm::SETTING, getName(),
	                           getValueString());
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

void Setting::info(TclObject& result) const
{
	result.addListElement(getTypeString());
	result.addListElement(getDefaultValueString());
	additionalInfo(result);
}

CommandController& Setting::getCommandController() const
{
	return commandController;
}

} // namespace openmsx
