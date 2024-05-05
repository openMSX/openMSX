#include "FirmwareSwitch.hh"
#include "MSXCliComm.hh"
#include "FileContext.hh"
#include "File.hh"
#include "FileException.hh"
#include <array>
#include <cstdint>

namespace openmsx {

static constexpr const char* const filename = "firmwareswitch";

FirmwareSwitch::FirmwareSwitch(const DeviceConfig& config_)
	: config(config_)
	, setting(
		config.getCommandController(), "firmwareswitch",
		"This setting controls the firmware switch",
		false, Setting::Save::NO)
{
	// load firmware switch setting from persistent data
	try {
		File file(config.getFileContext().resolveCreate(filename),
		          File::OpenMode::LOAD_PERSISTENT);
		std::array<uint8_t, 1> byteBuf;
		file.read(byteBuf);
		setting.setBoolean(byteBuf[0] != 0);
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't load firmwareswitch status: ", e.getMessage());
	}
}

FirmwareSwitch::~FirmwareSwitch()
{
	// save firmware switch setting value to persistent data
	try {
		File file(config.getFileContext().resolveCreate(filename),
		          File::OpenMode::SAVE_PERSISTENT);
		std::array byteBuf = {uint8_t(setting.getBoolean() ? 0xFF : 0x00)};
		file.write(byteBuf);
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't save firmwareswitch status: ", e.getMessage());
	}
}

} // namespace openmsx
