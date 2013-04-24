#include "Setting.hh"
#include "Observer.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "SettingsConfig.hh"
#include "TclObject.hh"
#include "CliComm.hh"
#include "XMLElement.hh"
#include "checked_cast.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

Setting::Setting(CommandController& commandController_, string_ref name_,
                 string_ref desc_, SaveSetting save_)
	: commandController(commandController_)
	, name       (name_.str())
	, description(desc_.str())
	, save(save_)
{
}

Setting::~Setting()
{
}

const string& Setting::getName() const
{
	return name;
}

string Setting::getDescription() const
{
	return description;
}

void Setting::changeValueString(const std::string& valueString)
{
	getCommandController().changeSetting(*this, valueString);
}

void Setting::notify() const
{
	Subject<Setting>::notify();
	commandController.getCliComm().update(
		CliComm::SETTING, getName(), getValueString());

	// Always keep SettingsConfig in sync.
	// TODO At the moment (partly because of this change) the whole Settings
	//  structure is more complicated than it could be. Though we're close
	//  to a release, so now is not the time to do big refactorings.
	sync(getGlobalCommandController().getSettingsConfig().getXMLElement());
}

void Setting::notifyPropertyChange() const
{
	TclObject result;
	info(result);
	commandController.getCliComm().update(
		CliComm::SETTINGINFO, getName(), result.getString());
}

bool Setting::needLoadSave() const
{
	return save == SAVE;
}
bool Setting::needTransfer() const
{
	return save != DONT_TRANSFER;
}

void Setting::setDontSaveValue(const std::string& dontSaveValue_)
{
	dontSaveValue = dontSaveValue_;
}

void Setting::sync(XMLElement& config) const
{
	auto& settings = config.getCreateChild("settings");
	if (!needLoadSave() || hasDefaultValue()) {
		// remove setting
		if (auto* elem = settings.findChildWithAttribute(
				"setting", "id", getName())) {
			settings.removeChild(*elem);
		}
	} else {
		// add (or overwrite) setting
		auto& elem = settings.getCreateChildWithAttribute(
				"setting", "id", getName());
		// check for non-saveable value
		// (mechanism can be generalize later when needed)
		string tmp = getValueString();
		if (tmp == dontSaveValue) tmp = getRestoreValueString();
		elem.setData(tmp);
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

GlobalCommandController& Setting::getGlobalCommandController() const
{
	if (auto globalCommandController =
	    dynamic_cast<GlobalCommandController*>(&commandController)) {
		return *globalCommandController;
	} else {
		return checked_cast<MSXCommandController*>(&commandController)
			->getGlobalCommandController();
	}
}

Interpreter& Setting::getInterpreter() const
{
	return getGlobalCommandController().getInterpreter();
}

} // namespace openmsx
