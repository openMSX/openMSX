// $Id$

#include <iostream>
#include <iterator>
#include <iomanip>
#include "EmuTime.hh"
#include "DebugDevice.hh"
#include "MSXConfig.hh"
#include "FileOperations.hh"
#include "Settings.hh"

namespace openmsx {

DebugDevice::DebugDevice(Device *config, const EmuTime & time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	mode = OFF;
	string outputFile;
	if (config->hasParameter("filename")) {
		outputFile = config->getParameter("filename");
	} else {
		outputFile = "stdout";
	}
	fileNameSetting = new FilenameSetting("debugoutput",
		"name of the file the debugdevice outputs to", outputFile);
	fileNameString = fileNameSetting->getValueString();
	openOutput(fileNameString);
}

DebugDevice::~DebugDevice()
{
	string name(fileNameSetting->getValueString());
	delete fileNameSetting;
	if ((name != "stderr") && (name != "stdout")) {
		debugOut.close();
	}
}

void DebugDevice::writeIO(byte port, byte value, const EmuTime& time)
{
	string currentName(fileNameSetting->getValueString());
	if (currentName != fileNameString) {
		closeOutput(fileNameString);
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
			flush();
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

void DebugDevice::flush()
{
	DisplayType dispType;
	if (mode == MULTIBYTE) {
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
		for (vector<byte>::const_iterator it = buffer.begin();
		     it != buffer.end();
		     ++it) {
			displayByte(*it, dispType);
		}
		buffer.clear();
		(*outputstrm) << endl;
	}
}

void DebugDevice::outputSingleByte(byte value, const EmuTime & time)
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
	(*outputstrm) << "emutime:" << time; 
	if ((modeParameter & 0x08) && ((value < ' ') || (value == 127))) {
		displayByte(value, ASC); // do special effects
	}
	(*outputstrm) << endl;
}

void DebugDevice::outputMultiByte(byte value)
{
	buffer.push_back(value);
}

void DebugDevice::displayByte(byte value, DisplayType type)
{
	switch (type) {
		case HEX:
			(*outputstrm) << hex << setw(2) << setfill('0')
			              << (int)value << "h ";
			break;
		case BIN: {
			byte mask = 128;
			while (mask != 0) {
				if (value & mask) {
					(*outputstrm) << "1";
				} else {
					(*outputstrm) << "0";
				}
				mask >>= 1;
			}
			(*outputstrm) << "b ";
			break;
		}
		case DEC:
			(*outputstrm) << dec << setw(3) << setfill('0')
			              << (int)value << " ";
			break;
		case ASC:
			(*outputstrm).put(value);
			break;
	}
}

void DebugDevice::openOutput(const string& name)
{
	if (name == "stdout") {
		outputstrm = &cout;
	} else if (name == "stderr") { 
		outputstrm = &cerr;
	} else {
		string realName = FileOperations::expandTilde(name);
		debugOut.open(realName.c_str(), std::ios::app);
		outputstrm = &debugOut;
	}
}

void DebugDevice::closeOutput(const string& name)
{
	if ((name != "stdout") && (name != "stderr")) {
		debugOut.close();
	}
}

} // namespace openmsx
