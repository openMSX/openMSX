#include "MSXTimer.hh"

#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "StringOp.hh"

#include <memory>

namespace openmsx {

/*****************************************************************************
 * MSX-TIMER
 *****************************************************************************/
MSXTimer::MSXTimer(const DeviceConfig& config)
	: MSXDevice(config)
	, timer0(config.getMotherBoard().getScheduler(), *this, FREQ, 0)
	, timer1(config.getMotherBoard().getScheduler(), *this, FREQ, 1)
	, timer2(config.getMotherBoard().getScheduler(), *this, FREQ, 2)
	, timer3(config.getMotherBoard().getScheduler(), *this, FREQ, 3)
	, irq(config.getMotherBoard(), getName() + ".IRQ")
	, ioEnabled(false)
{
	powerUp(getCurrentTime());
}

MSXTimer::~MSXTimer()
{
}

void MSXTimer::callback(int ch, bool state, EmuTime)
{
	if (state) {
		irqState |= 1 << ch;
	} else {
		irqState &= ~(1 << ch);
	}

	if (irqState != 0) {
		irq.set();
	} else {
		irq.reset();
	}
}

void MSXTimer::powerUp(EmuTime time)
{
	reset(time);
}

void MSXTimer::reset(EmuTime time)
{
	irq.reset();
	irqState = 0;
	registerLatch = 0;
	counterSelectLatch = 0;
	timer0.reset(time);
	timer1.reset(time);
	timer2.reset(time);
	timer3.reset(time);
	ioEnabled = false;
}

byte MSXTimer::readIO(uint16_t port, EmuTime time)
{
	if (!ioEnabled) return 0xFF;
	byte result = 0xFF;
	switch (port & 0x03) {
	case 0x00:
		result = registerLatch;
		break;
	case 0x01:
		switch ((registerLatch >> 2) & 3) {
		case 0:
			result = timer0.readReg(registerLatch, time);
			break;
		case 1:
			result = timer1.readReg(registerLatch, time);
			break;
		case 2:
			result = timer2.readReg(registerLatch, time);
			break;
		case 3:
			result = timer3.readReg(registerLatch, time);
			break;
		}
		break;
	case 0x02:
		result = (timer3.readIrqFlag(time) ? 0x08 : 0x00) |
				 (timer2.readIrqFlag(time) ? 0x04 : 0x00) |
				 (timer1.readIrqFlag(time) ? 0x02 : 0x00) |
				 (timer0.readIrqFlag(time) ? 0x01 : 0x00);
		break;
	case 0x03:
		switch (counterSelectLatch) {
		case 0x00:
			result = timer0.readCounter(time);
			break;
		case 0x01:
			result = timer1.readCounter(time);
			break;
		case 0x02:
			result = timer2.readCounter(time);
			break;
		case 0x03:
			result = timer3.readCounter(time);
			break;
		}
		break;
	}
	return result;
}

byte MSXTimer::peekIO(uint16_t port, EmuTime time) const
{
	if (!ioEnabled) return 0xFF;
	byte result = 0xFF;
	switch (port & 0x03) {
	case 0x00:
		result = registerLatch;
		break;
	case 0x01:
		switch ((registerLatch >> 2) & 3) {
		case 0:
			result = timer0.peekReg(registerLatch, time);
			break;
		case 1:
			result = timer1.peekReg(registerLatch, time);
			break;
		case 2:
			result = timer2.peekReg(registerLatch, time);
			break;
		case 3:
			result = timer3.peekReg(registerLatch, time);
			break;
		}
		break;
	case 0x02:
		result = (timer3.peekIrqFlag(time) ? 0x08 : 0x00) |
		   		 (timer2.peekIrqFlag(time) ? 0x04 : 0x00) |
			   	 (timer1.peekIrqFlag(time) ? 0x02 : 0x00) |
		   		 (timer0.peekIrqFlag(time) ? 0x01 : 0x00);
		break;
	case 0x03:
		switch (counterSelectLatch) {
		case 0x00:
			result = timer0.peekCounter(time);
			break;
		case 0x01:
			result = timer1.peekCounter(time);
			break;
		case 0x02:
			result = timer2.peekCounter(time);
			break;
		case 0x03:
			result = timer3.peekCounter(time);
			break;
		}
		break;
	}
	return result;
}

void MSXTimer::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (!ioEnabled) return;
	switch (port & 0x03) {
	case 0x00:
		registerLatch = value;
		break;
	case 0x01:
		switch ((registerLatch >> 2) & 3) {
		case 0:
			timer0.writeReg(registerLatch, value, time);
			break;
		case 1:
			timer1.writeReg(registerLatch, value, time);
			break;
		case 2:
			timer2.writeReg(registerLatch, value, time);
			break;
		case 3:
			timer3.writeReg(registerLatch, value, time);
			break;
		}
		break;
	case 0x02:
		if (value & 0x01) timer0.resetIrqFlag(time);
		if (value & 0x02) timer1.resetIrqFlag(time);
		if (value & 0x04) timer2.resetIrqFlag(time);
		if (value & 0x08) timer3.resetIrqFlag(time);
		break;
	case 0x03:
		// A value other than 0-3 selects no counter, and B3h then reads FFh.
		counterSelectLatch = (value & 0xFC) ? 0x04 : value;
		break;
	}
}

template<typename Archive>
void MSXTimer::serialize(Archive& ar, unsigned version)
{
	ar.serialize("timer0",         	   timer0,
				 "timer1",		 	   timer1,
				 "timer2",		 	   timer2,
				 "timer3",		 	   timer3,
				 "irqState",	   	   irqState,
	             "registerLatch", 	   registerLatch,
	             "counterSelectLatch", counterSelectLatch);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("ioEnabled", ioEnabled);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXTimer);
REGISTER_MSXDEVICE(MSXTimer, "MSX-TIMER");

/*****************************************************************************
 * timer core
 *****************************************************************************/
MSXTimerCore::MSXTimerCore(Scheduler& scheduler_, MSXTimerIrqCallback& cb_, unsigned freq, int chNum_)
	: Schedulable(scheduler_)
    , cb(cb_)
    , chNum(chNum_)
{
    clock.setFreq(freq);
}

void MSXTimerCore::executeUntil(EmuTime time)
{
    // reaching the sync point removed it
    scheduled = false;

    updateCounter(time);
    updateSchedule(time);
    updateIrq(time);
}

uint32_t MSXTimerCore::divCount(uint32_t value) const
{
    switch (ff_reso) {
    case 0:
        return value >> 10;
    case 1:
        return value >> 12;
    case 2:
        return value >> 14;
    case 3:
        return value >> 16;
    case 4:       
        return value >> 18;
    case 5:
        return value >> 20;
    case 6:
        return value >> 22;
    default:
        return value >> 24;
    }
}

uint64_t MSXTimerCore::mulCount(uint32_t value) const
{
    // 64 bits: a terminal value of 255 at resolution 7 asks for 256<<24,
    // which does not fit in 32. Wrapped to 0, the sync point that
    // getNextPoint() leads to would vanish and the counter would stop.
    uint64_t v = value;
    switch (ff_reso) {
    case 0:
        return (v << 10);
    case 1:
        return (v << 12);
    case 2:
        return (v << 14);
    case 3:
        return (v << 16);
    case 4:
        return (v << 18);
    case 5:
        return (v << 20);
    case 6:
        return (v << 22);
    default:
        return (v << 24);
    }
}

uint32_t MSXTimerCore::modCount(uint32_t value) const
{
    switch (ff_reso) {
    case 0:
        return (value & 0x000003FF);
    case 1:
        return (value & 0x00000FFF);
    case 2:
        return (value & 0x00003FFF);
    case 3:
        return (value & 0x0000FFFF);
    case 4:
        return (value & 0x0003FFFF);
    case 5:
        return (value & 0x000FFFFF);
    case 6:
        return (value & 0x003FFFFF);
    default:
        return (value & 0x00FFFFFF);
    }
}

bool MSXTimerCore::get_w_end_count(uint32_t value) const
{
    return ((divCount(value) & 0xFF) >= ff_count) && (modCount(value) == modCount(0xFFFFFFFF));
}

uint64_t MSXTimerCore::getNextPoint(uint32_t value) const
{
    uint32_t counter = divCount(value) & 0xFF;
    return mulCount((counter >= ff_count) ? (counter + 1) : ((uint32_t)ff_count + 1));
}

void MSXTimerCore::resetValue()
{
    irqState = false;
    ff_repeat = 0;
    ff_reso = 0;
    ff_intr_enable = 0;
    ff_count = 0;
    ff_count_enable = 0;
    ff_counter = 0;
    ff_count_end = 0;
}

void MSXTimerCore::updateSchedule(EmuTime /*time*/)
{
    uint64_t period = (!ff_repeat && get_w_end_count(ff_counter))
                      ?  0                                                  // one-shot, ff_count reached
                      : (getNextPoint(ff_counter) - (uint64_t)ff_counter);  // counting

    // a register write while counting leaves the old sync point behind
    if (scheduled) {
        removeSyncPoint();
        scheduled = false;
    }

    // Leave clock alone here. Moving it would make the elapsed count 0
    // when the sync point is reached, and the counter would not advance.
    if (ff_count_enable && period > 0) {
        setSyncPoint(clock + period);
        scheduled = true;
    }
}

void MSXTimerCore::updateIrq(EmuTime time)
{
    bool newIrq = ff_count_end && ff_intr_enable;
    if (irqState != newIrq) {
        cb.callback(chNum, newIrq, time);
    }
    irqState = newIrq;
}

void MSXTimerCore::checkCounter(uint32_t& counter, uint8_t& count_end, uint8_t& count_enable, EmuTime time) const
{
    counter = ff_counter;
    count_end = ff_count_end;
    count_enable = ff_count_enable;

    if (ff_count_enable) {
        uint64_t period = clock.getTicksTill(time);
        while (period > 0 && ff_count_enable) {
            // clocks up to the next carry out of the low bits (1..1 -> 0..0)
            uint64_t step = getNextPoint(counter) - (uint64_t)counter;
            if (step > period) step = period;
            assert(step > 0);

            uint64_t counter64 = (uint64_t)counter + step;

            if (get_w_end_count((uint32_t)(counter64 - 1))) {
                if (ff_repeat) {
                    counter64 = 0;
                } else {
                    counter64--;
                    count_enable = 0;
                }
                count_end = 1;
            }

            counter = (uint32_t)(counter64 & 0xFFFFFFFF);
            period -= step;
        }
    }
}

void MSXTimerCore::updateCounter(EmuTime time)
{
    uint32_t counter;
    uint8_t count_end;
    uint8_t count_enable;
    checkCounter(counter, count_end, count_enable, time);

    ff_counter = counter;
    ff_count_end = count_end;
    ff_count_enable = count_enable;

    // clock holds how far the counter has been brought up to date. Left
    // behind, checkCounter would count the same time twice. The fraction
    // of a tick carries over to the next update.
    clock.advance(time);
}

void MSXTimerCore::reset(EmuTime time)
{
    resetValue();
    clock.reset(time);
    updateSchedule(time);
}

bool MSXTimerCore::peekIrqFlag(EmuTime time) const
{
    uint32_t counter;
    uint8_t count_end;
    uint8_t count_enable;
    checkCounter(counter, count_end, count_enable, time);
    return count_end;
}

bool MSXTimerCore::readIrqFlag(EmuTime time)
{
    updateCounter(time);
    bool result = ff_count_end;
    updateSchedule(time);
    updateIrq(time);
    return result;
}

void MSXTimerCore::resetIrqFlag(EmuTime time)
{
    updateCounter(time);
    ff_count_end = 0;
    updateSchedule(time);
    updateIrq(time);
}

uint8_t MSXTimerCore::peekReg(uint8_t rg, EmuTime ) const
{
    switch (rg & 0x03) {
    case 0x00:
        return (ff_repeat & 1) |
               ((ff_reso & 0x07) << 4) |
               ((ff_intr_enable & 1) << 7);
    case 0x01:
        return ff_count;
    case 0x02:
        return ff_count_enable & 1;
    }
    return 0x00; // register 3 does not exist; the hardware reads it as 0
}

uint8_t MSXTimerCore::readReg(uint8_t rg, EmuTime time)
{
    updateCounter(time);
    uint8_t result = peekReg(rg, time);
    updateSchedule(time);
    updateIrq(time);
    return result;
}

void MSXTimerCore::writeReg(uint8_t rg, uint8_t data, EmuTime time)
{
    updateCounter(time);
 
    switch (rg & 0x03) {
    case 0x00:
        ff_repeat = data & 1;
        ff_reso = (data >> 4) & 0x07;
        ff_intr_enable = (data >> 7) & 1;
        break;
    case 0x01:
        ff_count = data;
        break;
    case 0x02:
        ff_count_enable = data & 1;
        if (data & 2) {
            ff_counter = 0;
        }
        break;
    }

    updateSchedule(time);
    updateIrq(time);
}

uint8_t MSXTimerCore::peekCounter(EmuTime time) const
{
    uint32_t counter;
    uint8_t count_end;
    uint8_t count_enable;
    checkCounter(counter, count_end, count_enable, time);
    return divCount(counter) & 0xFF;
}

uint8_t MSXTimerCore::readCounter(EmuTime time)
{
    updateCounter(time);
    uint8_t result = divCount(ff_counter) & 0xFF;
    updateSchedule(time);
    updateIrq(time);
    return result;
}

template<typename Archive>
void MSXTimerCore::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("scheduled",       scheduled,
                 "irqState",        irqState,
                 "ff_repeat",       ff_repeat,
	             "ff_reso",         ff_reso,
                 "ff_intr_enable",  ff_intr_enable,
                 "ff_count",        ff_count,
                 "ff_count_enable", ff_count_enable,
                 "ff_counter",      ff_counter,
                 "ff_count_end",    ff_count_end);
}
INSTANTIATE_SERIALIZE_METHODS(MSXTimerCore);

} // namespace openmsx
