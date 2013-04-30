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
		string_ref name, string_ref description,
		SaveSetting save)
	: Setting(commandController, name, description, save)
{
}

void SettingImplBase::init()
{
	if (needLoadSave()) {
		XMLElement& settingsConfig = Setting::getGlobalCommandController()
			.getSettingsConfig().getXMLElement();
		if (const XMLElement* config = settingsConfig.findChild("settings")) {
			if (const XMLElement* elem = config->findChildWithAttribute(
			                                "setting", "id", getName())) {
				try {
					setValueString2(elem->getData(), false);
				} catch (MSXException&) {
					// saved value no longer valid, just keep default
				}
			}
		}
	}
	Setting::getCommandController().registerSetting(*this);

	// This is needed to for example inform catapult of the new setting
	// value when a setting was destroyed/recreated (by a machine switch
	// for example).
	notify();
}

void SettingImplBase::destroy()
{
	sync(Setting::getGlobalCommandController()
		.getSettingsConfig().getXMLElement());
	Setting::getCommandController().unregisterSetting(*this);
}

void SettingImplBase::syncProxy()
{
	auto controller = dynamic_cast<MSXCommandController*>(
		&Setting::getCommandController());
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
	try {
		globalController.changeSetting(getName(), getValueString());
	} catch (MSXException&) {
		// ignore
	}
}


} // namespace openmsx
