// $Id$

#ifndef __DEBUG_DEVICE_
#define __DEBUG_DEVICE_

#include <fstream>
#include <memory>
#include "MSXDevice.hh"

namespace openmsx {

class EmuTime;
class FilenameSetting;

class DebugDevice : public MSXDevice
{
public:
	DebugDevice(const XMLElement& config, const EmuTime& time);
	virtual ~DebugDevice();
	
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	void openOutput(const std::string& name);
	void closeOutput(const std::string& name);
	
	enum DisplayType {HEX, BIN, DEC, ASC};
	enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

private:
	void outputSingleByte(byte value, const EmuTime& time);
	void outputMultiByte(byte value);
	void displayByte(byte value, DisplayType type);
	
	enum DebugMode mode;
	byte modeParameter;
	std::auto_ptr<FilenameSetting> fileNameSetting;
	std::ostream* outputstrm;
	std::ofstream debugOut;
	std::string fileNameString;
};

} // namespace openmsx

#endif
