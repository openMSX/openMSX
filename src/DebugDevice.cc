#include "DebugDevice.hh"
#include "Clock.hh"
#include "FileOperations.hh"
#include "serialize.hh"
#include <iostream>
#include <iomanip>

using std::string;

namespace openmsx {

DebugDevice::DebugDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, fileNameSetting(
		getCommandController(), "debugoutput",
		"name of the file the debugdevice outputs to",
		config.getChildData("filename", "stdout"))
{
	openOutput(fileNameSetting.getString());
	reset(EmuTime::dummy());
}

void DebugDevice::reset(EmuTime::param /*time*/)
{
	mode = OFF;
	modeParameter = 0;
}

void DebugDevice::writeIO(word port, byte value, EmuTime::param time)
{
	const auto& newName = fileNameSetting.getString();
	if (newName != fileNameString) {
		openOutput(newName);
	}

	switch (port & 0x01) {
	case 0:
		switch ((value & 0x30) >> 4) {
		case 0:
			mode = OFF;
			break;
		case 1:
			mode = SINGLEBYTE;
			modeParameter = value & 0x0F;
			break;
		case 2:
			mode = MULTIBYTE;
			modeParameter = value & 0x03;
			break;
		case 3:
			break;
		}
		if (!(value & 0x40)){
			(*outputstrm) << std::endl;
		}
		break;
	case 1:
		switch (mode) {
		case OFF:
			break;
		case SINGLEBYTE:
			outputSingleByte(value, time);
			break;
		case MULTIBYTE:
			outputMultiByte(value);
		default:
			break;
		}
		break;
	}
}

void DebugDevice::outputSingleByte(byte value, EmuTime::param time)
{
	if (modeParameter & 0x01) {
		displayByte(value, HEX);
	}
	if (modeParameter & 0x02) {
		displayByte(value, BIN);
	}
	if (modeParameter & 0x04) {
		displayByte(value, DEC);
	}
	if (modeParameter & 0x08) {
		(*outputstrm) << '\'';
		byte tmp = ((value >= ' ') && (value != 127)) ? value : '.';
		displayByte(tmp, ASC);
		(*outputstrm) << "' ";
	}
	Clock<3579545> zero(EmuTime::zero);
	(*outputstrm) << "emutime: " << std::dec << zero.getTicksTill(time);
	if ((modeParameter & 0x08) && ((value < ' ') || (value == 127))) {
		displayByte(value, ASC); // do special effects
	}
	(*outputstrm) << std::endl;
}

void DebugDevice::outputMultiByte(byte value)
{
	DisplayType dispType;
	switch (modeParameter) {
	case 0:
		dispType = HEX;
		break;
	case 1:
		dispType = BIN;
		break;
	case 2:
		dispType = DEC;
		break;
	case 3:
	default:
		dispType = ASC;
		break;
	}
	displayByte(value, dispType);
}

void DebugDevice::displayByte(byte value, DisplayType type)
{
	switch (type) {
	case HEX:
		(*outputstrm) << std::hex << std::setw(2)
		              << std::setfill('0')
		              << int(value) << "h " << std::flush;
		break;
	case BIN: {
		for (byte mask = 0x80; mask; mask >>= 1) {
			(*outputstrm) << ((value & mask) ? '1' : '0');
		}
		(*outputstrm) << "b " << std::flush;
		break;
	}
	case DEC:
		(*outputstrm) << std::dec << std::setw(3)
		              << std::setfill('0')
		              << int(value) << ' ' << std::flush;
		break;
	case ASC:
		(*outputstrm).put(value);
		(*outputstrm) << std::flush;
		break;
	}
}

void DebugDevice::openOutput(string_ref name)
{
	fileNameString = name.str();
	debugOut.close();
	if (name == "stdout") {
		outputstrm = &std::cout;
	} else if (name == "stderr") {
		outputstrm = &std::cerr;
	} else {
		string realName = FileOperations::expandTilde(name);
		FileOperations::openofstream(debugOut, realName, std::ios::app);
		outputstrm = &debugOut;
	}
}

static std::initializer_list<enum_string<DebugDevice::DebugMode>> debugModeInfo = {
	{ "OFF",        DebugDevice::OFF },
	{ "SINGLEBYTE", DebugDevice::SINGLEBYTE },
	{ "MULTIBYTE",  DebugDevice::MULTIBYTE },
	{ "ASCII",      DebugDevice::ASCII }
};
SERIALIZE_ENUM(DebugDevice::DebugMode, debugModeInfo);

template<typename Archive>
void DebugDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("mode", mode);
	ar.serialize("modeParameter", modeParameter);
}
INSTANTIATE_SERIALIZE_METHODS(DebugDevice);
REGISTER_MSXDEVICE(DebugDevice, "DebugDevice");

} // namespace openmsx
