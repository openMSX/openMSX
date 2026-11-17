#ifndef Y8960CARTRIDGE_HH
#define Y8960CARTRIDGE_HH

#include "RomBlocks.hh"

#include "AY8910.hh"
#include "Ram.hh"
#include "SCC.hh"
#include "Y8950.hh"
#include "Y8950Periphery.hh"
#include "YM2413.hh"

#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <string>

namespace openmsx {

/** HRA!'s Y8960 sound cartridge: a Konami SCC style mapper with 128kB of RAM
  * next to the ROM, and the registers of its sound chips and digital mixer.
  * The top of Y8960Cartridge.cc has where the CPU finds them.
  */
class Y8960Cartridge final : public Rom8kBBlocks
{
public:
	Y8960Cartridge(const DeviceConfig& config, Rom&& rom);
	~Y8960Cartridge() override;

	// MSXDevice
	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** The blocks of the digital mixer, each with its own volume register. */
	enum class Block : uint8_t { SSG, OPLL, OPL2, SCC, OPM, NUM };

	// mapper
	[[nodiscard]] uint8_t getBank(unsigned region) const { return bankReg[region - 2]; }
	[[nodiscard]] bool isRamRegion(unsigned region) const;
	[[nodiscard]] bool isSccRegion(unsigned region) const;
	[[nodiscard]] bool isSccAddress(uint16_t address) const;
	[[nodiscard]] bool isMmioVisible() const;
	[[nodiscard]] bool isIoEnabler2(uint16_t address) const;
	[[nodiscard]] unsigned getRamAddress(uint16_t address) const;
	void setBank(unsigned region, uint8_t value);
	void writeMmio(uint16_t address, byte value, EmuTime time);
	void writeIoEnabler1(uint8_t value);
	void writeIoEnabler2(uint8_t value);

	// sound chips, port 0 address, port 1 data
	void writeOpll(bool port, uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t readOpl2(bool port, EmuTime time);
	[[nodiscard]] uint8_t peekOpl2(bool port, EmuTime time) const;
	void writeOpl2(bool port, uint8_t value, EmuTime time);
	void writeSsg(bool port, uint8_t value, EmuTime time);

	// digital mixer
	void writeVolume(Block block, uint8_t value, EmuTime time);
	void writeMixer(unsigned reg, uint8_t value, EmuTime time);
	void applyVolume(Block block, EmuTime time);
	void applyPan(unsigned reg, EmuTime time);
	void applyAll(EmuTime time);

private:
	SCC scc;
	YM2413 opll;
	struct NoPeriphery final : Y8950Periphery {
		void write(uint4_t outputs, uint4_t values, EmuTime time) override;
		[[nodiscard]] uint4_t read(EmuTime time) override;
	} noPeriphery;
	Y8950 opl2;
	std::array<AY8910, 2> ssg;

	// the registers of the digital mixer, laid out as in Y8960Cartridge.cc
	std::array<uint8_t, 0x2E> mixer{};
	struct MixerDebuggable final : SimpleDebuggable {
		MixerDebuggable(MSXMotherBoard& motherBoard, const std::string& name);
		using SimpleDebuggable::read;
		using SimpleDebuggable::write;
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} mixerDebuggable;

	Ram ram;
	std::array<uint8_t, 4> bankReg = {0, 1, 2, 3};
	bool ramEnabled = false;
	uint8_t ioEnabler1 = 0;
	uint8_t ioEnabler2 = 0;
	uint8_t opllLatch = 0;
	uint8_t opl2Latch = 0;
	uint8_t ssgLatch = 0;
};

} // namespace openmsx

#endif
