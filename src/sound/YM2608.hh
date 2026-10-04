#ifndef YM2608_HH
#define YM2608_HH

#include "AY8910.hh"
#include "ResampledSoundDevice.hh"

#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "Ram.hh"
#include "SimpleDebuggable.hh"

#include "3rdparty/ymfm/ymfm_opn.h"

#include <array>

namespace openmsx {

class YM2608 final : private ymfm::ymfm_interface
{
public:
	YM2608(DeviceConfig& config, std::string_view name, EmuTime time);

	void reset(EmuTime time);
	uint8_t readPort(unsigned port, EmuTime time);
	uint8_t peekPort(unsigned port, EmuTime time) const;
	void writePort(unsigned port, uint8_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t readStatus();
	uint8_t readData(EmuTime time);
	uint8_t readStatusHi();
	uint8_t statusHi() const;
	uint8_t readDataHi();

	void writeRegister(unsigned regnum, uint8_t data, EmuTime time);
	uint8_t peekRegister(unsigned regnum, EmuTime time) const;

	void updateStream(EmuTime time);
	void updatePrescale(uint8_t prescale);

	[[nodiscard]] unsigned prescale() const;
	[[nodiscard]] unsigned fmRate() const;
	[[nodiscard]] unsigned ssgRate() const;
	void applyRates();

	// ymfm_interface
	void ymfm_set_timer(uint32_t timer, int32_t duration) override;
	void ymfm_set_busy_end(uint32_t duration) override;
	bool ymfm_is_busy() override;
	void ymfm_update_irq(bool asserted) override;
	uint8_t ymfm_external_peek(ymfm::access_class type, uint32_t address) override;
	uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override;
	void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t value) override;

	template<bool Combined>
	void generateFM(std::span<float*> buffers, unsigned num);

private:
	class FmPart final : public ResampledSoundDevice {
	public:
		FmPart(DeviceConfig& config, std::string_view name, YM2608& chip);
		~FmPart();

		void updateStream(EmuTime time); // expose SoundDevice::updateStream()
		void restoreClock(EmuTime time);
		void rate(unsigned value);
		void setOutputRate(unsigned rate, double speed) override;

	private:
		void generateChannels(std::span<float*> buffers, unsigned num) override;

	private:
		YM2608& chip;
	};

	class Timer final : public Schedulable {
	public:
		Timer(Scheduler& scheduler, YM2608& ym2608, uint8_t index);
		void cancel();
		void schedule(EmuTime time);
		template<typename Archive> void serialize(Archive& ar, unsigned version);

	private:
		void executeUntil(EmuTime time) override;

	private:
		YM2608& ym2608;
		uint8_t index;
	};

	struct Registers final : SimpleDebuggable {
		Registers(MSXMotherBoard& board, std::string_view name, YM2608& ym2608);
		uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;

	private:
		YM2608& ym2608;
	};

	IRQHelper irq;
	std::array<Timer, 2> timers;
	EmuTime contextTime;
	EmuTime busyEnd;

	uint16_t addressLatch = 0;
	uint8_t irqEnable = 0x1f;
	uint8_t flagControl = 0x1c;

	using fm_engine = ymfm::fm_engine_base<ymfm::opna_registers>;
	fm_engine fm;
	ymfm::adpcm_a_engine adpcmA;
	ymfm::adpcm_b_engine adpcmB;

	Ram sampleRAM;
	Registers registers;
	FmPart fmPart;
	AY8910 ssg; // ym2149
};

} // namespace openmsx

#endif
