// $Id$

#include "DebugDevice.hh"
#include "MSXConfig.hh"
#include "FileOperations.hh"
#include <iostream>
#include <iterator>
#include <iomanip>

DebugDevice::DebugDevice(Device *config, const EmuTime & time)
		: MSXDevice(config, time), MSXIODevice(config, time)
{
	mode = OFF;
	std::string outputFile;
	if (config->hasParameter("filename")){
		outputFile = config->getParameter("filename");
	}
	else{
		outputFile = "stdout";
	}
	fileNameSetting = new FilenameSetting ("debugoutput","name of the file the debugdevice outputs to",outputFile);
	fileNameString = fileNameSetting->getValueString();
	openOutput (fileNameString);
}

DebugDevice::~DebugDevice ()
{
	std::string name = fileNameSetting->getValueString();
	delete fileNameSetting;
	if ((name != "stderr") && (name != "stdout")){
	debugOut.close();
	}
}

void DebugDevice::writeIO (byte port, byte value, const EmuTime & time)
{
	std::string currentName = fileNameSetting->getValueString();
	if (currentName != fileNameString){
		closeOutput(fileNameString);
		fileNameString = currentName;
		openOutput (fileNameString);
	}
	switch (port & 0x01)
	{
		case 0:
		switch ((value & 0x30) >> 4)
		{
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
			switch (mode)
			{
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
	enum DebugDevice::DisplayType dispType;
	if (mode == MULTIBYTE){
		switch (modeParameter){
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
		std::vector <byte>::iterator it;
		for (it=buffer.begin();it!=buffer.end();it++){
			displayByte (*it, dispType);
		}
		buffer.clear();
		(*outputstrm) << std::endl;
	}
}

void DebugDevice::outputSingleByte(byte value, const EmuTime & time)
{
	if (modeParameter & 0x01){
		displayByte (value, HEX);
	}
	if (modeParameter & 0x02){
		displayByte (value, BIN);
	}
	if (modeParameter & 0x04){
		displayByte (value, DEC);
	}
	if (modeParameter & 0x08){
		(*outputstrm) << "'";
		if ((value>=' ') && (value!=127)){
			displayByte (value,ASC);
		}
		else{
			displayByte ('.',ASC);
		}
		(*outputstrm) << "' ";
	}
	(*outputstrm) << "emutime:" << time; 
	if ((modeParameter & 0x08) && ((value < ' ') || (value == 127))){
		displayByte (value, ASC); // do special effects
	}
	(*outputstrm) << std::endl;
}

void DebugDevice::outputMultiByte(byte value)
{
	buffer.push_back(value);
}

void DebugDevice::displayByte (byte value, enum DebugDevice::DisplayType type)
{
	switch (type){
		case HEX:
			(*outputstrm) << std::hex << std::setw(2) << std::setfill('0') << (int)value << "h ";
			break;
		case BIN:
			{
				byte mask = 128;
				while (mask != 0){
					if (value & mask){
						(*outputstrm) << "1";
					}
					else{
						(*outputstrm) << "0";
					}
				mask >>=1;
				}
				(*outputstrm) << "b ";
			}
			break;
		case DEC:
			(*outputstrm) << std::dec << std::setw(3) << std::setfill('0') << (int)value << " ";
			break;
		case ASC:
			(*outputstrm).put(value);
			break;
	}
}

void DebugDevice::openOutput (std::string name)
{
	if (name == "stdout") {
		outputstrm = &std::cout;
	}
	else if (name == "stderr") { 
		outputstrm = &std::cerr;
	}else {
	std::string realName = FileOperations::expandTilde(name);
	debugOut.open(realName.c_str(), std::ios_base::app);
	outputstrm = &debugOut;
	}
}

void DebugDevice::closeOutput (std::string name)
{
	if ((name != "stdout") && (name != "stderr")){
		debugOut.close();
	}
}
