// $Id$

#ifndef __DEBUG_DEVICE_
#define __DEBUG_DEVICE_

#include "Settings.hh"
#include "openmsx.hh"
#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include <vector>
#include <fstream>

class DebugDevice : public MSXIODevice
{
	public:
		DebugDevice(Device *config, const EmuTime & time);
		~DebugDevice();
		enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};
		enum DisplayType {HEX, BIN, DEC, ASC};
		void writeIO(byte port, byte value, const EmuTime & time);
		void openOutput (std::string name);
		void closeOutput (std::string name);

	private:
		enum DebugMode mode;
		std::ostream * outputstrm;
		byte modeParameter;
		FilenameSetting * fileNameSetting;
		std::ofstream debugOut;
		std::string fileNameString;
		std::vector <byte> buffer;
		void outputSingleByte(byte value,const EmuTime & time);
		void outputMultiByte (byte value);
		void displayByte (byte value, enum DebugDevice::DisplayType);
		void flush ();
};

#endif
