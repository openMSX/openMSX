// $Id$

#ifndef __DEBUG_DEVICE_
#define __DEBUG_DEVICE_

#include "MSXIODevice.hh"
#include <vector>
#include <fstream>

class EmuTime;
class FilenameSetting;

using namespace std;

class DebugDevice : public MSXIODevice
{
	public:
		DebugDevice(Device *config, const EmuTime& time);
		~DebugDevice();
		
		void writeIO(byte port, byte value, const EmuTime& time);
		void openOutput(const string& name);
		void closeOutput(const string& name);
		
		enum DisplayType {HEX, BIN, DEC, ASC};
		enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

	private:
		void outputSingleByte(byte value, const EmuTime& time);
		void outputMultiByte(byte value);
		void displayByte(byte value, enum DebugDevice::DisplayType);
		void flush();
		
		enum DebugMode mode;
		byte modeParameter;
		FilenameSetting* fileNameSetting;
		ostream* outputstrm;
		ofstream debugOut;
		string fileNameString;
		vector<byte> buffer;
};

#endif
