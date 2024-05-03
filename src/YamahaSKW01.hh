#ifndef YAMAHASKW01_HH
#define YAMAHASKW01_HH

#include "MSXDevice.hh"
#include "Connector.hh"
#include "SRAM.hh"
#include "Rom.hh"
#include <array>
#include <optional>

namespace openmsx {

class PrinterPortDevice;

class YamahaSKW01PrinterPort final : public Connector
{
public:
	YamahaSKW01PrinterPort(PluggingController& pluggingController, const std::string& name);

	// printer port functionality
	void reset(EmuTime::param time);
	[[nodiscard]] bool getStatus(EmuTime::param time) const;
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(uint8_t newData, EmuTime::param time);

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

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
	explicit YamahaSKW01(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;

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
