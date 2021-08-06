#include "Setting.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "SettingsConfig.hh"
#include "TclObject.hh"
#include "CliComm.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "checked_cast.hh"

namespace openmsx {

// class BaseSetting

BaseSetting::BaseSetting(std::string_view name)
	: fullName(name)
	, baseName(fullName)
{
}

BaseSetting::BaseSetting(TclObject name)
	: fullName(std::move(name))
	, baseName(fullName)
{
}

void BaseSetting::info(TclObject& result) const
{
	result.addListElement(getTypeString(), getDefaultValue());
	additionalInfo(result);
}


// class Setting

Setting::Setting(CommandController& commandController_,
                 std::string_view name, static_string_view description_,
                 const TclObject& initialValue, SaveSetting save_)
	: BaseSetting(name)
	, commandController(commandController_)
	, description(description_)
	, value(initialValue)
	, defaultValue(initialValue)
	, restoreValue(initialValue)
	, save(save_)
{
	checkFunc = [](TclObject&) { /* nothing */ };
}

void Setting::init()
{
	if (needLoadSave()) {
		auto& settingsConfig = getGlobalCommandController()
			.getSettingsConfig().getXMLElement();
		if (auto* config = settingsConfig.findChild("settings")) {
			if (auto* elem = config->findChildWithAttribute(
			                                "setting", "id", getBaseName())) {
				try {
					setValueDirect(TclObject(elem->getData()));
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


std::string_view Setting::getDescription() const
{
	return description;
}

void Setting::setValue(const TclObject& newValue)
{
	getInterpreter().setVariable(getFullNameObj(), newValue);
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
	TclObject val = getValue();
	commandController.getCliComm().update(
		CliComm::SETTING, getBaseName(), val.getString());

	// Always keep SettingsConfig in sync.
	auto& config = getGlobalCommandController().getSettingsConfig().getXMLElement();
	auto& settings = config.getCreateChild("settings");
	if (!needLoadSave() || (val == getDefaultValue())) {
		// remove setting
		if (auto* elem = settings.findChildWithAttribute(
				"setting", "id", getBaseName())) {
			settings.removeChild(*elem);
		}
	} else {
		// add (or overwrite) setting
		auto& elem = settings.getCreateChildWithAttribute(
				"setting", "id", getBaseName());
		// check for non-saveable value
		// (mechanism can be generalize later when needed)
		if (val == dontSaveValue) val = getRestoreValue();
		elem.setData(val.getString());
	}
}

void Setting::notifyPropertyChange() const
{
	TclObject result;
	info(result);
	commandController.getCliComm().update(
		CliComm::SETTINGINFO, getBaseName(), result.getString());
}

bool Setting::needLoadSave() const
{
	return save == SAVE;
}
bool Setting::needTransfer() const
{
	return save != DONT_TRANSFER;
}

void Setting::setDontSaveValue(const TclObject& dontSaveValue_)
{
	dontSaveValue = dontSaveValue_;
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

void Setting::tabCompletion(std::vector<std::string>& /*tokens*/) const
{
	// nothing
}

void Setting::additionalInfo(TclObject& /*result*/) const
{
	// nothing
}


void Setting::setValueDirect(const TclObject& newValue_)
{
	TclObject newValue = newValue_;
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

	// Tcl already makes sure this doesn't result in an endless loop.
	try {
		getInterpreter().setVariable(getBaseNameObj(), getValue());
	} catch (MSXException&) {
		// ignore
	}
}

} // namespace openmsx
