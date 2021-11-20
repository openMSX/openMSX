#ifndef DEBUGDEVICE_HH
#define DEBUGDEVICE_HH

#include "MSXDevice.hh"
#include "FilenameSetting.hh"
#include <fstream>

namespace openmsx {

class DebugDevice final : public MSXDevice
{
public:
	explicit DebugDevice(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum DebugMode {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

private:
	enum DisplayType {HEX, BIN, DEC, ASC};

	void outputSingleByte(byte value, EmuTime::param time);
	void outputMultiByte(byte value);
	void displayByte(byte value, DisplayType type);
	void openOutput(std::string_view name);

private:
	FilenameSetting fileNameSetting;
	std::ostream* outputstrm;
	std::ofstream debugOut;
	std::string fileNameString;
	DebugMode mode;
	byte modeParameter;
};

} // namespace openmsx

#endif
