#include "ReadOnlySetting.hh"
#include "MSXException.hh"

namespace openmsx {

ReadOnlySetting::ReadOnlySetting(
		CommandController& commandController_,
		string_ref name_, string_ref description_,
		const TclObject& initialValue)
	: Setting(commandController_, name_, description_, initialValue,
	          Setting::DONT_TRANSFER)
	, roValue(initialValue)
{
	setChecker([this](TclObject& newValue) {
		if (newValue != roValue) {
			throw MSXException("Read-only setting");
		}
	});
	init();
}

void ReadOnlySetting::setReadOnlyValue(const TclObject& newValue)
{
	roValue = newValue;
	setValue(newValue);
}

string_ref ReadOnlySetting::getTypeString() const
{
	return "read-only";
}

} // namespace openmsx
