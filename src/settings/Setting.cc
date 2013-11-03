#include "Setting.hh"
#include "Observer.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "SettingsConfig.hh"
#include "TclObject.hh"
#include "CliComm.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "checked_cast.hh"
#include <algorithm>
#include <cassert>

using std::string;

namespace openmsx {

// class BaseSetting

BaseSetting::BaseSetting(string_ref name_)
	: name       (name_.str())
{
}

const string& BaseSetting::getName() const
{
	return name;
}

void BaseSetting::info(TclObject& result) const
{
	result.addListElement(getTypeString());
	result.addListElement(getDefaultValue());
	additionalInfo(result);
}


// class Setting

Setting::Setting(CommandController& commandController_,
                 string_ref name_, string_ref desc_,
                 const string& initialValue, SaveSetting save_)
	: BaseSetting(name_)
	, commandController(commandController_)
	, description(desc_.str())
	, value(TclObject(getInterpreter(), initialValue))
	, defaultValue(initialValue)
	, restoreValue(initialValue)
	, save(save_)
{
	checkFunc = [](TclObject&) { /* nothing */ };

	if (needLoadSave()) {
		auto& settingsConfig = getGlobalCommandController()
			.getSettingsConfig().getXMLElement();
		if (auto* config = settingsConfig.findChild("settings")) {
			if (auto* elem = config->findChildWithAttribute(
			                                "setting", "id", getName())) {
				try {
					setStringDirect(elem->getData());
				} catch (MSXException&) {
					// saved value no longer valid, just keep default
				}
			}
		}
	}
	getCommandController().registerSetting(*this);

	// This is needed to for example inform catapult of the new setting
	// value when a setting was destroyed/recreated (by a machine switch
	// for example).
	notify();
}

Setting::~Setting()
{
	getCommandController().unregisterSetting(*this);
}


string Setting::getDescription() const
{
	return description;
}

void Setting::setString(const string& value)
{
	getCommandController().changeSetting(*this, value);
}

void Setting::notify() const
{
	// Notify all subsystems of a change in the setting value. There
	// are actually quite a few subsystems involved with the settings:
	//  - the setting classes themselves
	//  - the Tcl variables (and possibly traces on those variables)
	//  - Subject/Observers
	//  - CliComm setting-change events (for external GUIs)
	//  - SettingsConfig (keeps values, also of not yet created settings)
	// This method takes care of the last 3 in this list.
	Subject<Setting>::notify();
	commandController.getCliComm().update(
		CliComm::SETTING, getName(), getString());

	// Always keep SettingsConfig in sync.
	auto& config = getGlobalCommandController().getSettingsConfig().getXMLElement();
	auto& settings = config.getCreateChild("settings");
	string value = getString();
	if (!needLoadSave() || (value == getDefaultValue())) {
		// remove setting
		if (auto* elem = settings.findChildWithAttribute(
				"setting", "id", getName())) {
			settings.removeChild(*elem);
		}
	} else {
		// add (or overwrite) setting
		auto& elem = settings.getCreateChildWithAttribute(
				"setting", "id", getName());
		elem.setData(value);
	}
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

CommandController& Setting::getCommandController() const
{
	return commandController;
}

GlobalCommandController& Setting::getGlobalCommandController() const
{
	if (auto* globalCommandController =
	    dynamic_cast<GlobalCommandController*>(&commandController)) {
		return *globalCommandController;
	} else {
		return checked_cast<MSXCommandController*>(&commandController)
			->getGlobalCommandController();
	}
}

Interpreter& Setting::getInterpreter() const
{
	return commandController.getInterpreter();
}

void Setting::tabCompletion(std::vector<string>& /*tokens*/) const
{
	// nothing
}

void Setting::additionalInfo(TclObject& /*result*/) const
{
	// nothing
}

void Setting::setRestoreValue(const string& value)
{
	restoreValue = value;
}

void Setting::setChecker(std::function<void(TclObject&)> checkFunc_)
{
	checkFunc = checkFunc_;
}


string Setting::getString() const
{
	return value.getString().str();
}

string Setting::getDefaultValue() const
{
	return defaultValue;
}

string Setting::getRestoreValue() const
{
	return restoreValue;
}

void Setting::setStringDirect(const string& str)
{
	TclObject newValue(getInterpreter(), str);
	checkFunc(newValue);
	if (newValue != value) {
		value = newValue;
		notify();
	}

	// synchronize proxy
	auto* controller = dynamic_cast<MSXCommandController*>(
		&getCommandController());
	if (!controller) {
		// This is not a machine specific setting.
		return;
	}
	if (!controller->isActive()) {
		// This setting does not belong to the active machine.
		return;
	}

	auto& globalController = controller->getGlobalCommandController();
	// Tcl already makes sure this doesn't result in an endless loop.
	try {
		globalController.changeSetting(getName(), getString());
	} catch (MSXException&) {
		// ignore
	}
}

} // namespace openmsx
