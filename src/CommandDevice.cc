#include "CommandDevice.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "serialize.hh"

using std::string;

namespace openmsx {

static const byte IO_ID = 30; // openMSX cmd ctl
static const byte STATUS_IDLE = 0xFF;

CommandDevice::CommandDevice(const DeviceConfig& config)
: MSXDevice(config), MSXSwitchedDevice(getMotherBoard(), IO_ID)
{
	reset(EmuTime::dummy());
}

void CommandDevice::reset(EmuTime::param /*time*/)
{
	commandRequest.clear();
	commandResponse.clear();
	commandResponseIndex = 0;
	commandStatus = STATUS_IDLE;
}

byte CommandDevice::readSwitchedIO(word port, EmuTime::param time)
{
	if (commandResponseIndex >= commandResponse.length()) {
		commandStatus = STATUS_IDLE;
	}
	byte result = peekSwitchedIO(port, time);
	if ((port & 0x0F) == 2) {
		commandResponseIndex++;
	}
	return result;
}

byte CommandDevice::peekSwitchedIO(word port, EmuTime::param /*time*/) const
{
	byte result = 0xFF;
	switch (port & 0x0F) {
		case 0: // the complement of the current device ID
			result = byte(~IO_ID);
			break;
		case 1: // read command status
			result = commandStatus;
			break;
		case 2: // read response byte (and auto increment index)
			if (!commandResponse.length() == 0) {
				result = commandResponse.at(commandResponseIndex);
			}
			break;
	}
	return result;
}

void CommandDevice::writeSwitchedIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x0F) {
		case 1: // append request char
			commandRequest.append(1, value);
			break;
		case 2: // exe request
			commandRequest.append(1, '\n');
			commandExecute();
			commandRequest.clear();
			break;
		case 3: // clear request
			commandRequest.clear();
			break;
		case 4: // manual set response index
			commandResponseIndex = value;
			break;
	}
}

void CommandDevice::commandExecute()
{
	commandResponseIndex = 0;
	commandResponse.clear();
	try {
		auto resultObj = getCommandController().executeCommand(commandRequest);
		commandStatus = 0;
		commandResponse.append(resultObj.getString().str());
	} catch (CommandException& e) {
		commandStatus = 1;
		commandResponse.append(e.getMessage());
	}

	// insert 'carriage returns' after 'new lines' for msx ascii output
	auto newLinePos = commandResponse.find('\n');
	while (newLinePos != std::string::npos) {
		commandResponse.insert(newLinePos, "\r");
		newLinePos = commandResponse.find('\n', newLinePos + 2);
	}
}

template<typename Archive>
void CommandDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(CommandDevice);
REGISTER_MSXDEVICE(CommandDevice, "CommandDevice");

} // namespace openmsx
