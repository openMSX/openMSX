// $Id$

#ifndef DEBUGDEVICE_HH
#define DEBUGDEVICE_HH

#include "MSXDevice.hh"
#include <fstream>
#include <memory>

namespace openmsx {

class EmuTime;
class FilenameSetting;

class DebugDevice : public MSXDevice
{
public:
	DebugDevice(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~DebugDevice();

	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

private:
	enum DisplayType {HEX, BIN, DEC, ASC};

	void outputSingleByte(byte value, const EmuTime& time);
	void outputMultiByte(byte value);
	void displayByte(byte value, DisplayType type);
	void openOutput(const std::string& name);

	std::auto_ptr<FilenameSetting> fileNameSetting;
	std::ostream* outputstrm;
	std::ofstream debugOut;
	std::string fileNameString;
	DebugMode mode;
	byte modeParameter;
};

} // namespace openmsx

#endif
