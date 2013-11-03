#include "ProxySetting.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"

using std::string;
using std::vector;

namespace openmsx {

ProxySetting::ProxySetting(Reactor& reactor_, string_ref name)
	: BaseSetting(name)
	, reactor(reactor_)
{
}

BaseSetting* ProxySetting::getSetting()
{
	auto* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return nullptr;
	return motherBoard->getMSXCommandController().findSetting(getName());
}

const BaseSetting* ProxySetting::getSetting() const
{
	return const_cast<ProxySetting*>(this)->getSetting();
}

void ProxySetting::setString(const std::string& value)
{
	if (auto* setting = getSetting()) {
		setting->setString(value);
	}
}

string_ref ProxySetting::getTypeString() const
{
	if (auto* setting = getSetting()) {
		return setting->getTypeString();
	} else {
		return "proxy";
	}
}

std::string ProxySetting::getDescription() const
{
	if (auto* setting = getSetting()) {
		return setting->getDescription();
	} else {
		return "proxy";
	}
}

std::string ProxySetting::getString() const
{
	if (auto* setting = getSetting()) {
		return setting->getString();
	} else {
		throw MSXException("No setting '" + getName() + "' on current machine.");
	}
}

std::string ProxySetting::getDefaultValue() const
{
	if (auto* setting = getSetting()) {
		return setting->getDefaultValue();
	} else {
		return "proxy";
	}
}

std::string ProxySetting::getRestoreValue() const
{
	if (auto* setting = getSetting()) {
		return setting->getRestoreValue();
	} else {
		return "proxy";
	}
}

void ProxySetting::setStringDirect(const string& value)
{
	if (auto* setting = getSetting()) {
		// note: not setStringDirect()
		setting->setString(value);
	} else {
		throw MSXException("No setting '" + getName() + "' on current machine.");
	}
}

void ProxySetting::tabCompletion(vector<string>& tokens) const
{
	if (auto* setting = getSetting()) {
		setting->tabCompletion(tokens);
	}
}

bool ProxySetting::needLoadSave() const
{
	if (auto* setting = getSetting()) {
		return setting->needLoadSave();
	} else {
		return false;
	}
}

bool ProxySetting::needTransfer() const
{
	if (auto* setting = getSetting()) {
		return setting->needTransfer();
	} else {
		return false;
	}
}

void ProxySetting::setDontSaveValue(const std::string& dontSaveValue)
{
	if (auto* setting = getSetting()) {
		setting->setDontSaveValue(dontSaveValue);
	}
}

void ProxySetting::additionalInfo(TclObject& result) const
{
	if (auto* setting = getSetting()) {
		setting->additionalInfo(result);
	}
}

} // namespace openmsx
