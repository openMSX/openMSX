#include "ReadOnlySetting.hh"
#include "MSXException.hh"

namespace openmsx {

ReadOnlySetting::ReadOnlySetting(
		CommandController& commandController,
		string_ref name, string_ref description,
		const TclObject& initialValue)
	: Setting(commandController, name, description, initialValue,
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

void ReadOnlySetting::setReadOnlyValue(const TclObject& value)
{
	roValue = value;
	setValue(value);
}

string_ref ReadOnlySetting::getTypeString() const
{
	return "read-only";
}

} // namespace openmsx
