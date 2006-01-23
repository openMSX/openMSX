// $Id$

#include <iostream>
#include <iomanip>
#include "Clock.hh"
#include "DebugDevice.hh"
#include "FileOperations.hh"
#include "FilenameSetting.hh"
#include "XMLElement.hh"
#include "MSXMotherBoard.hh"

using std::string;

namespace openmsx {

DebugDevice::DebugDevice(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
	mode = OFF;
	string outputFile = config.getChildData("filename", "stdout");
	fileNameSetting.reset(new FilenameSetting(
		motherBoard.getCommandController(), "debugoutput",
		"name of the file the debugdevice outputs to", outputFile));
	fileNameString = fileNameSetting->getValueString();
	openOutput(fileNameString);
}

DebugDevice::~DebugDevice()
{
}

void DebugDevice::writeIO(word port, byte value, const EmuTime& time)
{
	string currentName = fileNameSetting->getValueString();
	if (currentName != fileNameString) {
		fileNameString = currentName;
		openOutput(fileNameString);
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

void DebugDevice::outputSingleByte(byte value, const EmuTime& time)
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
		(*outputstrm) << "'";
		if ((value >= ' ') && (value != 127)) {
			displayByte(value, ASC);
		} else {
			displayByte('.', ASC);
		}
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
			              << (int)value << "h " << std::flush;
			break;
		case BIN: {
			byte mask = 128;
			while (mask != 0) {
				if (value & mask) {
					(*outputstrm) << "1" << std::flush;
				} else {
					(*outputstrm) << "0" << std::flush;
				}
				mask >>= 1;
			}
			(*outputstrm) << "b " << std::flush;
			break;
		}
		case DEC:
			(*outputstrm) << std::dec << std::setw(3)
			              << std::setfill('0')
			              << (int)value << " " << std::flush;
			break;
		case ASC:
			(*outputstrm).put(value);
			break;
	}
}

void DebugDevice::openOutput(const string& name)
{
	debugOut.close();
	if (name == "stdout") {
		outputstrm = &std::cout;
	} else if (name == "stderr") {
		outputstrm = &std::cerr;
	} else {
		string realName = FileOperations::expandTilde(name);
		debugOut.open(realName.c_str(), std::ios::app);
		outputstrm = &debugOut;
	}
}

} // namespace openmsx
