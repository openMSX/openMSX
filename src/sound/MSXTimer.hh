#ifndef MSXTIMER_HH
#define MSXTIMER_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "DynamicClock.hh"
#include "Schedulable.hh"
#include <memory>
#include <string>

namespace openmsx {

class MSXTimerIrqCallback
{
public:
	virtual void callback(int ch, bool state, EmuTime time) = 0;

protected:
	~MSXTimerIrqCallback() = default;
};

class MSXTimerCore final : public Schedulable
{
public:
	MSXTimerCore(Scheduler& scheduler, MSXTimerIrqCallback& cb, unsigned freq, int chNum_);

    void reset(EmuTime time);
   	bool peekIrqFlag(EmuTime time) const;
    bool readIrqFlag(EmuTime time);
   	void resetIrqFlag(EmuTime time);
    uint8_t peekReg(uint8_t rg, EmuTime time) const;
   	uint8_t readReg(uint8_t rg, EmuTime time);
    void writeReg(uint8_t rg, uint8_t data, EmuTime time);
   	uint8_t peekCounter(EmuTime time) const;
    uint8_t readCounter(EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void executeUntil(EmuTime time) override;

    void resetValue();
   	uint32_t divCount(uint32_t value) const;
    uint32_t mulCount(uint32_t value) const;
   	uint32_t modCount(uint32_t value) const;
    bool get_w_end_count(uint32_t value) const;
   	uint64_t getNextPoint(uint32_t value) const;
    void updateSchedule(EmuTime time);
   	void updateIrq(EmuTime time);
    void checkCounter(uint32_t& counter, uint8_t& count_end, uint8_t& count_enable, EmuTime time) const;
   	void updateCounter(EmuTime time);

private:
	MSXTimerIrqCallback& cb;
	DynamicClock clock{EmuTime::zero()};
    int chNum;
	bool scheduled = false;
    bool irqState = false;
   	uint8_t ff_repeat;
    uint8_t ff_reso;
   	uint8_t ff_intr_enable;
    uint8_t ff_count;
   	uint8_t ff_count_enable;
    unsigned ff_counter;
   	uint8_t ff_count_end;
};

class MSXTimer final : public MSXDevice, private MSXTimerIrqCallback
{
public:
	const unsigned FREQ = 3579545 * 24;
	explicit MSXTimer(const DeviceConfig& config);
	~MSXTimer() override;

	/** Creates a periphery object for this MSXAudio cartridge.
	  * The ownership of the object remains with the MSXAudio instance.
	  */
	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	//[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	//[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	//void writeMem(uint16_t address, byte value, EmuTime time) override;
	//[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	//[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void callback(int ch, bool state, EmuTime time) override;

public:

private:
	IRQHelper irq;
	uint8_t irqState = 0;
	MSXTimerCore timer0;
	MSXTimerCore timer1;
	MSXTimerCore timer2;
	MSXTimerCore timer3;
	byte registerLatch;
	byte counterSelectLatch;
};

} // namespace openmsx

#endif
