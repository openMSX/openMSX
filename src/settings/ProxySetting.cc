#include "ProxySetting.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"

using std::string;
using std::vector;

namespace openmsx {

ProxySetting::ProxySetting(Reactor& reactor_, const TclObject& name)
	: BaseSetting(name)
	, reactor(reactor_)
{
}

BaseSetting* ProxySetting::getSetting()
{
	auto* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return nullptr;
	auto& manager = reactor.getGlobalCommandController().getSettingsManager();
	auto& controller = motherBoard->getMSXCommandController();
	return manager.findSetting(controller.getPrefix(), getFullName());
}

const BaseSetting* ProxySetting::getSetting() const
{
	return const_cast<ProxySetting*>(this)->getSetting();
}

void ProxySetting::setValue(const TclObject& value)
{
	if (auto* setting = getSetting()) {
		setting->setValue(value);
	}
}

std::string_view ProxySetting::getTypeString() const
{
	if (const auto* setting = getSetting()) {
		return setting->getTypeString();
	} else {
		return "proxy";
	}
}

std::string_view ProxySetting::getDescription() const
{
	if (const auto* setting = getSetting()) {
		return setting->getDescription();
	} else {
		return "proxy";
	}
}

const TclObject& ProxySetting::getValue() const
{
	if (const auto* setting = getSetting()) {
		return setting->getValue();
	} else {
		throw MSXException("No setting '", getFullName(), "' on current machine.");
	}
}

std::optional<TclObject> ProxySetting::getOptionalValue() const
{
	if (const auto* setting = getSetting()) {
		return setting->getOptionalValue();
	} else {
		return {};
	}
}

TclObject ProxySetting::getDefaultValue() const
{
	if (const auto* setting = getSetting()) {
		return setting->getDefaultValue();
	} else {
		return TclObject("proxy");
	}
}

TclObject ProxySetting::getRestoreValue() const
{
	if (const auto* setting = getSetting()) {
		return setting->getRestoreValue();
	} else {
		return TclObject("proxy");
	}
}

void ProxySetting::setValueDirect(const TclObject& value)
{
	if (auto* setting = getSetting()) {
		// note: not setStringDirect()
		setting->setValue(value);
	} else {
		throw MSXException("No setting '", getFullName(), "' on current machine.");
	}
}

void ProxySetting::tabCompletion(vector<string>& tokens) const
{
	if (const auto* setting = getSetting()) {
		setting->tabCompletion(tokens);
	}
}

bool ProxySetting::needLoadSave() const
{
	if (const auto* setting = getSetting()) {
		return setting->needLoadSave();
	} else {
		return false;
	}
}

bool ProxySetting::needTransfer() const
{
	if (const auto* setting = getSetting()) {
		return setting->needTransfer();
	} else {
		return false;
	}
}

void ProxySetting::setDontSaveValue(const TclObject& dontSaveValue)
{
	if (auto* setting = getSetting()) {
		setting->setDontSaveValue(dontSaveValue);
	}
}

void ProxySetting::additionalInfo(TclObject& result) const
{
	if (const auto* setting = getSetting()) {
		setting->additionalInfo(result);
	}
}

} // namespace openmsx
