#include "ReadOnlySetting.hh"
#include "MSXException.hh"

namespace openmsx {

ReadOnlySetting::ReadOnlySetting(
		CommandController& commandController,
		string_ref name, string_ref description,
		const std::string& initialValue)
	: Setting(commandController, name, description, initialValue,
	          Setting::DONT_TRANSFER)
	, roValue(initialValue)
{
	setChecker([this](TclObject& newValue) {
		if (newValue.getString() != roValue) {
			throw MSXException("Read-only setting");
		}
	});
	init();
}

void ReadOnlySetting::setReadOnlyValue(const std::string& value)
{
	roValue = value;
	setString(value);
}

string_ref ReadOnlySetting::getTypeString() const
{
	return "read-only";
}

} // namespace openmsx
