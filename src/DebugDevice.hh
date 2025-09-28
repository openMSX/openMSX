#ifndef DEBUGDEVICE_HH
#define DEBUGDEVICE_HH

#include "FilenameSetting.hh"
#include "MSXDevice.hh"

#include <cstdint>
#include <fstream>

namespace openmsx {

class DebugDevice final : public MSXDevice
{
public:
	explicit DebugDevice(const DeviceConfig& config);

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum class Mode : uint8_t {OFF, SINGLEBYTE, MULTIBYTE, ASCII};

private:
	enum class DisplayType : uint8_t {HEX, BIN, DEC, ASC};

	void outputSingleByte(byte value, EmuTime time);
	void outputMultiByte(byte value);
	void displayByte(byte value, DisplayType type);
	void openOutput(std::string_view name);

private:
	FilenameSetting fileNameSetting;
	std::ostream* outputStrm;
	std::ofstream debugOut;
	std::string fileNameString;
	Mode mode;
	byte modeParameter;
};

} // namespace openmsx

#endif
