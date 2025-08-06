#include "DebugDevice.hh"

#include "Clock.hh"
#include "serialize.hh"

#include "FileOperations.hh"

#include "narrow.hh"
#include "strCat.hh"

#include <iostream>
#include <iomanip>

namespace openmsx {

DebugDevice::DebugDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, fileNameSetting(
		getCommandController(), tmpStrCat(getName(), " output"),
		"name of the file the debug-device outputs to",
		config.getChildData("filename", "stdout"))
{
	openOutput(fileNameSetting.getString());
	reset(EmuTime::dummy());
}

void DebugDevice::reset(EmuTime /*time*/)
{
	mode = Mode::OFF;
	modeParameter = 0;
}

void DebugDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (const auto& newName = fileNameSetting.getString();
	    newName != fileNameString) {
		openOutput(newName);
	}

	switch (port & 0x01) {
	case 0:
		switch ((value & 0x30) >> 4) {
		case 0:
			mode = Mode::OFF;
			break;
		case 1:
			mode = Mode::SINGLEBYTE;
			modeParameter = value & 0x0F;
			break;
		case 2:
			mode = Mode::MULTIBYTE;
			modeParameter = value & 0x03;
			break;
		case 3:
			break;
		}
		if (!(value & 0x40)){
			(*outputStrm) << '\n' << std::flush;
		}
		break;
	case 1:
		switch (mode) {
		case Mode::OFF:
			break;
		case Mode::SINGLEBYTE:
			outputSingleByte(value, time);
			break;
		case Mode::MULTIBYTE:
			outputMultiByte(value);
			break;
		default:
			break;
		}
		break;
	}
}

void DebugDevice::outputSingleByte(byte value, EmuTime time)
{
	if (modeParameter & 0x01) {
		displayByte(value, DisplayType::HEX);
	}
	if (modeParameter & 0x02) {
		displayByte(value, DisplayType::BIN);
	}
	if (modeParameter & 0x04) {
		displayByte(value, DisplayType::DEC);
	}
	if (modeParameter & 0x08) {
		(*outputStrm) << '\'';
		byte tmp = ((value >= ' ') && (value != 127)) ? value : '.';
		displayByte(tmp, DisplayType::ASC);
		(*outputStrm) << "' ";
	}
	Clock<3579545> zero(EmuTime::zero());
	(*outputStrm) << "emutime: " << std::dec << zero.getTicksTill(time);
	if ((modeParameter & 0x08) && ((value < ' ') || (value == 127))) {
		displayByte(value, DisplayType::ASC); // do special effects
	}
	(*outputStrm) << '\n' << std::flush;
}

void DebugDevice::outputMultiByte(byte value)
{
	DisplayType dispType = [&] {
		switch (modeParameter) {
			case 0:  return DisplayType::HEX;
			case 1:  return DisplayType::BIN;
			case 2:  return DisplayType::DEC;
			case 3:
			default: return DisplayType::ASC;
		}
	}();
	displayByte(value, dispType);
}

void DebugDevice::displayByte(byte value, DisplayType type)
{
	switch (type) {
	case DisplayType::HEX:
		(*outputStrm) << std::hex << std::setw(2)
		              << std::setfill('0')
		              << int(value) << "h " << std::flush;
		break;
	case DisplayType::BIN: {
		for (byte mask = 0x80; mask; mask >>= 1) {
			(*outputStrm) << ((value & mask) ? '1' : '0');
		}
		(*outputStrm) << "b " << std::flush;
		break;
	}
	case DisplayType::DEC:
		(*outputStrm) << std::dec << std::setw(3)
		              << std::setfill('0')
		              << int(value) << ' ' << std::flush;
		break;
	case DisplayType::ASC:
		(*outputStrm).put(narrow_cast<char>(value));
		(*outputStrm) << std::flush;
		break;
	}
}

void DebugDevice::openOutput(std::string_view name)
{
	fileNameString = name;
	debugOut.close();
	if (name == "stdout") {
		outputStrm = &std::cout;
	} else if (name == "stderr") {
		outputStrm = &std::cerr;
	} else {
		auto realName = FileOperations::expandTilde(fileNameString);
		FileOperations::openOfStream(debugOut, realName, std::ios::app);
		outputStrm = &debugOut;
	}
}

static constexpr std::initializer_list<enum_string<DebugDevice::Mode>> debugModeInfo = {
	{ "OFF",        DebugDevice::Mode::OFF },
	{ "SINGLEBYTE", DebugDevice::Mode::SINGLEBYTE },
	{ "MULTIBYTE",  DebugDevice::Mode::MULTIBYTE },
	{ "ASCII",      DebugDevice::Mode::ASCII }
};
SERIALIZE_ENUM(DebugDevice::Mode, debugModeInfo);

template<typename Archive>
void DebugDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("mode",          mode,
	             "modeParameter", modeParameter);
}
INSTANTIATE_SERIALIZE_METHODS(DebugDevice);
REGISTER_MSXDEVICE(DebugDevice, "DebugDevice");

} // namespace openmsx
