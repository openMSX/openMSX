#ifndef DEBUGDEVICE_HH
#define DEBUGDEVICE_HH

#include "MSXDevice.hh"
#include <fstream>
#include <memory>

namespace openmsx {

class FilenameSetting;

class DebugDevice : public MSXDevice
{
public:
	explicit DebugDevice(const DeviceConfig& config);
	virtual ~DebugDevice();

	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

private:
	enum DisplayType {HEX, BIN, DEC, ASC};

	void outputSingleByte(byte value, EmuTime::param time);
	void outputMultiByte(byte value);
	void displayByte(byte value, DisplayType type);
	void openOutput(const std::string& name);

	std::unique_ptr<FilenameSetting> fileNameSetting;
	std::ostream* outputstrm;
	std::ofstream debugOut;
	std::string fileNameString;
	DebugMode mode;
	byte modeParameter;
};

} // namespace openmsx

#endif
