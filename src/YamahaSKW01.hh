#ifndef YAMAHASKW01_HH
#define YAMAHASKW01_HH

#include "Connector.hh"
#include "MSXDevice.hh"
#include "Rom.hh"
#include "SRAM.hh"

#include <array>
#include <optional>

namespace openmsx {

class PrinterPortDevice;

class YamahaSKW01PrinterPort final : public Connector
{
public:
	YamahaSKW01PrinterPort(PluggingController& pluggingController, const std::string& name);

	// printer port functionality
	void reset(EmuTime time);
	[[nodiscard]] bool getStatus(EmuTime time) const;
	void setStrobe(bool newStrobe, EmuTime time);
	void writeData(uint8_t newData, EmuTime time);

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] PrinterPortDevice& getPluggedPrintDev() const;


private:
	bool strobe = false; // != true
	uint8_t data = 255;  // != 0
};

class YamahaSKW01 final : public MSXDevice
{
public:
	explicit YamahaSKW01(DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom mainRom;
	Rom fontRom;
	Rom dataRom;
	SRAM sram;
	std::array<uint16_t, 4> fontAddress;
	uint16_t dataAddress;

	std::optional<YamahaSKW01PrinterPort> printerPort;
};

} // namespace openmsx

#endif
