// $Id$

#include "FirmwareSwitch.hh"
#include "BooleanSetting.hh"
#include "CliComm.hh"
#include "DeviceConfig.hh"
#include "FileContext.hh"
#include "File.hh"
#include "FileException.hh"

using std::string;

namespace openmsx {

static const char* const filename = "firmwareswitch";

FirmwareSwitch::FirmwareSwitch(const DeviceConfig& config_)
	: config(config_)
	, setting(new BooleanSetting(config.getCommandController(),
	          "firmwareswitch",
	          "This setting controls the firmware switch",
	          false, Setting::DONT_SAVE))
{
	// load firmware switch setting from persistent data
	try {
		File file(config.getFileContext().resolveCreate(filename),
		          File::LOAD_PERSISTENT);
		byte bytebuf;
		file.read(&bytebuf, 1);
		setting->changeValue(bytebuf != 0);
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't load firmwareswitch status: " + e.getMessage());
	}
}

FirmwareSwitch::~FirmwareSwitch()
{
	// save firmware switch setting value to persistent data
	try {
		File file(config.getFileContext().resolveCreate(filename),
		          File::SAVE_PERSISTENT);
		byte bytebuf = setting->getValue() ? 0xFF : 0x00;
		file.write(&bytebuf, 1);
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't save firmwareswitch status: " + e.getMessage());
	}
}

bool FirmwareSwitch::getStatus() const
{
	return setting->getValue();
}

} // namespace openmsx
