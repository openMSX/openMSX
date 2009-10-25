// $Id$

#include "SettingImpl.hh"
#include "SettingsConfig.hh"
#include "XMLElement.hh"
#include "CommandController.hh"
#include "MSXCommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXException.hh"

namespace openmsx {

SettingImplBase::SettingImplBase(
		CommandController& commandController,
		const std::string& name, const std::string& description,
		SaveSetting save)
	: Setting(commandController, name, description, save)
{
}

void SettingImplBase::init()
{
	CommandController& commandController = Setting::getCommandController();
	if (needLoadSave()) {
		XMLElement& settingsConfig =
			commandController.getSettingsConfig().getXMLElement();
		const XMLElement* config = settingsConfig.findChild("settings");
		if (config) {
			const XMLElement* elem = config->findChildWithAttribute(
				"setting", "id", getName());
			if (elem) {
				try {
					setValueString2(elem->getData(), false);
				} catch (MSXException&) {
					// saved value no longer valid, just keep default
				}
			}
		}
	}
	commandController.registerSetting(*this);

	// This is needed to for example inform catapult of the new setting
	// value when a setting was destroyed/recreated (by a machine switch
	// for example).
	notify();
}

void SettingImplBase::destroy()
{
	CommandController& commandController = Setting::getCommandController();
	sync(commandController.getSettingsConfig().getXMLElement());
	commandController.unregisterSetting(*this);
}

void SettingImplBase::syncProxy()
{
	MSXCommandController* controller =
		dynamic_cast<MSXCommandController*>(&Setting::getCommandController());
	if (!controller) {
		// This is not a machine specific setting.
		return;
	}
	if (!controller->isActive()) {
		// This setting does not belong to the active machine.
		return;
	}

	GlobalCommandController& globalController =
		controller->getGlobalCommandController();
	// Tcl already makes sure this doesn't result in an endless loop.
	globalController.changeSetting(getName(), getValueString());
}


} // namespace openmsx
