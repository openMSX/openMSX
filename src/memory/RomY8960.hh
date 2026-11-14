#ifndef ROMY8960_HH
#define ROMY8960_HH

#include "RomBlocks.hh"
#include "Ram.hh"
#include "Y8960SCC.hh"
#include "MSXTimer.hh"
#include "Y8960Mixer.hh"
#include "Y8960OPL2Device.hh"
#include "Y8960SSGSDevice.hh"
#include "Y8960OPLLDevice.hh"
#include "Y8960OPMDevice.hh"

#include <string>

namespace openmsx {

class RomY8960 final : public Rom8kBBlocks
{
public:
	static constexpr int RomBankCounts = 16;
	static constexpr int RamBankCounts = 16;

	RomY8960(const DeviceConfig& config, Rom&& rom);

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void bankSwitch(unsigned page, unsigned block);
	void writeMmio(uint16_t address, byte value, EmuTime time);
	[[nodiscard]] Y8960Mixer* getMixer();
	bool isRamRegion(unsigned int region) const;
	bool isSccRegion(unsigned int region) const;
	bool isSccAddress(uint16_t address) const;
	static bool isSccPanAddress(uint16_t address);
	bool isMmioVisible() const;
	bool isIoEnabler2Address(uint16_t address) const;
	[[nodiscard]] byte peekIoEnabler2() const;
	uint8_t getBank(unsigned int region) const;
	void setBank(unsigned int region, uint8_t bank);
	const unsigned int getRamAddress(uint16_t address) const;
	unsigned int convAddressToRegion(uint16_t address) const;

private:
	Y8960OPLLDevice *opll;
	Y8960OPL2Device *opl2;
	Y8960SSGSDevice *ssgs;
	Y8960OPMDevice *opm;
	MSXTimer *timer;
	Y8960Mixer* mixer = nullptr;
	std::string mixerName;
	Y8960SCC scc;
	Ram ram;
	bool ramEnabled;
	std::array<uint8_t, 4> bankReg;

	static constexpr uint8_t SccBank = 0x3F;
};
SERIALIZE_CLASS_VERSION(RomY8960, 2);

} // namespace openmsx

#endif
