#ifndef Y8960CARTRIDGE_HH
#define Y8960CARTRIDGE_HH

#include "RomBlocks.hh"

#include "AY8910.hh"
#include "MSXSwitchedDevice.hh"
#include "Ram.hh"
#include "SCC.hh"
#include "Y8950.hh"
#include "Y8950Periphery.hh"
#include "YM2151.hh"
#include "YM2413.hh"

#include "DynamicClock.hh"
#include "IRQHelper.hh"
#include "Schedulable.hh"
#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <string>

namespace openmsx {

/** HRA!'s Y8960 sound cartridge: a Konami SCC style mapper with 128kB of RAM
  * next to the ROM, and the registers of its sound chips and digital mixer.
  * The top of Y8960Cartridge.cc has where the CPU finds them.
  */
class Y8960Cartridge final : public Rom8kBBlocks, public MSXSwitchedDevice
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

	// MSXSwitchedDevice, the System Controller
	[[nodiscard]] uint8_t readSwitchedIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekSwitchedIO(uint16_t port, EmuTime time) const override;
	void writeSwitchedIO(uint16_t port, uint8_t value, EmuTime time) override;

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
	[[nodiscard]] uint8_t readOpm(bool port) const;
	void writeOpm(bool port, uint8_t value, EmuTime time);

	// digital mixer
	void writeVolume(Block block, uint8_t value, EmuTime time);
	void writeMixer(unsigned reg, uint8_t value, EmuTime time);
	void applyVolume(Block block, EmuTime time);
	void applyPan(unsigned reg, EmuTime time);
	void applyAll(EmuTime time);

private:
	/** The MSX-TIMER: four counters that count up to a terminal value, each
	  * with its interrupt, behind four ports:
	  *   0: register index (bits 3-2 the counter, bits 1-0 its register)
	  *   1: register data
	  *   2: interrupt flags, bit n for counter n; writing 1 clears a flag
	  *   3: write: which counter port 3 reads (0-3, anything else none);
	  *      read: that counter's count, 0xFF for none
	  * The registers of a counter:
	  *   0: bit 0 repeat, bits 6-4 resolution, bit 7 interrupt enable
	  *   1: terminal value
	  *   2: bit 0 counting, writing bit 1 clears the count
	  *   3: not there, reads 0
	  */
	class Timer
	{
	public:
		// The timer's design sheet gives 85.9MHz, but the cartridge clocks
		// it with its 42.95MHz system clock.
		static constexpr unsigned CLOCK_FREQ = 3579545 * 12;

		Timer(MSXMotherBoard& motherBoard, const std::string& name);

		void reset(EmuTime time);
		[[nodiscard]] uint8_t readPort(unsigned port, EmuTime time);
		[[nodiscard]] uint8_t peekPort(unsigned port, EmuTime time) const;
		void writePort(unsigned port, uint8_t value, EmuTime time);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		class Counter final : public Schedulable {
		public:
			Counter(Timer& timer, Scheduler& scheduler);

			void reset(EmuTime time);
			[[nodiscard]] uint8_t peekReg(unsigned reg) const;
			void writeReg(unsigned reg, uint8_t value, EmuTime time);
			void clearFlag(EmuTime time);
			void sync(EmuTime time);
			[[nodiscard]] uint8_t peekCount(EmuTime time) const;
			[[nodiscard]] bool peekFlag(EmuTime time) const;
			[[nodiscard]] bool flag() const { return ended; }
			[[nodiscard]] bool interrupt() const { return ended && intEnable; }

			template<typename Archive>
			void serialize(Archive& ar, unsigned version);

		private:
			void executeUntil(EmuTime time) override;
			[[nodiscard]] unsigned shift() const { return 10 + 2 * resolution; }
			[[nodiscard]] uint8_t count(uint64_t t) const { return uint8_t(t >> shift()); }
			void advance(uint64_t& t, bool& end, bool& on, uint64_t elapsed) const;
			void schedule();

			Timer& timer;
			DynamicClock clock{EmuTime::zero()};
			uint64_t ticks = 0; // since the count was last cleared
			uint8_t terminal = 0;
			uint8_t resolution = 0;
			bool repeat = false;
			bool intEnable = false;
			bool counting = false;
			bool ended = false;
		};

		void updateIrq();

		std::array<Counter, 4> counters;
		IRQHelper irq;
		uint8_t index = 0;
		uint8_t select = 0; // 4: none
	};

	SCC scc;
	YM2413 opll;
	struct NoPeriphery final : Y8950Periphery {
		void write(uint4_t outputs, uint4_t values, EmuTime time) override;
		[[nodiscard]] uint4_t read(EmuTime time) override;
	} noPeriphery;
	Y8950 opl2;
	std::array<AY8910, 2> ssg;
	YM2151 opm;
	Timer timer;

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
	uint8_t opmLatch = 0;

	// System Controller
	bool boardSelected = false;
	bool romFunction = false;
};

} // namespace openmsx

#endif
