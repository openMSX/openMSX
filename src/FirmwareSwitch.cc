// $Id$

#include "FirmwareSwitch.hh"
#include "BooleanSetting.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "File.hh"

using std::string;

namespace openmsx {

static const std::string filename = "firmwareswitch";

FirmwareSwitch::FirmwareSwitch(CommandController& commandController,
                                 const XMLElement& config_)
	: setting(new BooleanSetting(commandController, "firmwareswitch",
	          "This setting controls the firmware switch",
	          false, Setting::DONT_SAVE))
	, config(config_)
	, cliComm(commandController.getCliComm())
{
	// load firmware switch setting from persistent data
	try {
		File file(config.getFileContext().resolveCreate(filename),
			  File::LOAD_PERSISTENT);
		byte bytebuf;
		file.read(&bytebuf, 1);
		setting->setValue(bytebuf != 0);
	} catch (FileException &e) {
		cliComm.printWarning("Couldn't load firmwareswitch status"
		                     " (" + e.getMessage() + ").");
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
		cliComm.printWarning("Couldn't save firmwareswitch status" 
		                     " (" + e.getMessage() + ").");
	}

}

bool FirmwareSwitch::getStatus() const
{
	return setting->getValue();
}

} // namespace openmsx
