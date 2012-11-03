// $Id$

#include "ProxySetting.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"

using std::string;
using std::vector;

namespace openmsx {

ProxySetting::ProxySetting(CommandController& commandController,
                           Reactor& reactor_,
                           string_ref name)
	: Setting(commandController, name, "proxy", DONT_SAVE)
	, reactor(reactor_)
{
}

Setting* ProxySetting::getSetting()
{
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return nullptr;
	return motherBoard->getMSXCommandController().findSetting(getName());
}

const Setting* ProxySetting::getSetting() const
{
	return const_cast<ProxySetting*>(this)->getSetting();
}

string_ref ProxySetting::getTypeString() const
{
	if (const Setting* setting = getSetting()) {
		return setting->getTypeString();
	} else {
		return "proxy";
	}
}

string ProxySetting::getDescription() const
{
	if (const Setting* setting = getSetting()) {
		return setting->getDescription();
	} else {
		return "proxy";
	}
}

string ProxySetting::getValueString() const
{
	if (const Setting* setting = getSetting()) {
		return setting->getValueString();
	} else {
		throw MSXException("No setting '" + getName() + "' on current machine.");
	}
}

string ProxySetting::getDefaultValueString() const
{
	if (const Setting* setting = getSetting()) {
		return setting->getDefaultValueString();
	} else {
		return "proxy";
	}
}

string ProxySetting::getRestoreValueString() const
{
	if (const Setting* setting = getSetting()) {
		return setting->getRestoreValueString();
	} else {
		return "proxy";
	}
}

void ProxySetting::setValueStringDirect(const string& valueString)
{
	if (Setting* setting = getSetting()) {
		// note: not setValueStringDirect()
		setting->changeValueString(valueString);
	} else {
		throw MSXException("No setting '" + getName() + "' on current machine.");
	}
}

void ProxySetting::restoreDefault()
{
	if (Setting* setting = getSetting()) {
		setting->restoreDefault();
	}
}

bool ProxySetting::hasDefaultValue() const
{
	if (const Setting* setting = getSetting()) {
		return setting->hasDefaultValue();
	} else {
		return true;
	}
}

void ProxySetting::tabCompletion(vector<string>& tokens) const
{
	if (const Setting* setting = getSetting()) {
		setting->tabCompletion(tokens);
	}
}

bool ProxySetting::needLoadSave() const
{
	if (const Setting* setting = getSetting()) {
		return setting->needLoadSave();
	} else {
		return false;
	}
}

void ProxySetting::setDontSaveValue(const string& dontSaveValue)
{
	if (Setting* setting = getSetting()) {
		setting->setDontSaveValue(dontSaveValue);
	}
}

void ProxySetting::additionalInfo(TclObject& result) const
{
	if (const Setting* setting = getSetting()) {
		setting->additionalInfo(result);
	}
}

} // namespace openmsx
