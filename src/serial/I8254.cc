#include "I8254.hh"
#include "EmuTime.hh"
#include "ClockPin.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <cassert>

namespace openmsx {

static const byte READ_BACK = 0xC0;
static const byte RB_CNTR0  = 0x02;
static const byte RB_CNTR1  = 0x04;
static const byte RB_CNTR2  = 0x08;
static const byte RB_STATUS = 0x10;
static const byte RB_COUNT  = 0x20;


class Counter {
public:
	Counter(Scheduler& scheduler, ClockPinListener* listener,
		EmuTime::param time);
	void reset(EmuTime::param time);
	byte readIO(EmuTime::param time);
	byte peekIO(EmuTime::param time) const;
	void writeIO(byte value, EmuTime::param time);
	void setGateStatus(bool status, EmuTime::param time);
	void writeControlWord(byte value, EmuTime::param time);
	void latchStatus(EmuTime::param time);
	void latchCounter(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

//private:
	enum ByteOrder {LOW, HIGH};

private:
	static const byte WRT_FRMT = 0x30;
	static const byte WF_LATCH = 0x00;
	static const byte WF_LOW   = 0x10;
	static const byte WF_HIGH  = 0x20;
	static const byte WF_BOTH  = 0x30;
	static const byte CNTR_MODE = 0x0E;
	static const byte CNTR_M0   = 0x00;
	static const byte CNTR_M1   = 0x02;
	static const byte CNTR_M2   = 0x04;
	static const byte CNTR_M3   = 0x06;
	static const byte CNTR_M4   = 0x08;
	static const byte CNTR_M5   = 0x0A;
	static const byte CNTR_M2_  = 0x0C;
	static const byte CNTR_M3_  = 0x0E;

	void writeLoad(word value, EmuTime::param time);
	void advance(EmuTime::param time);

	ClockPin clock;
	ClockPin output;
	EmuTime currentTime;
	int counter;
	word latchedCounter, counterLoad;
	byte control, latchedControl;
	bool ltchCtrl, ltchCntr;
	ByteOrder readOrder, writeOrder;
	byte writeLatch;
	bool gate;
	bool active, triggered, counting;

	friend class I8254;
};


// class I8254

I8254::I8254(Scheduler& scheduler, ClockPinListener* output0,
             ClockPinListener* output1, ClockPinListener* output2,
             EmuTime::param time)
{
	counter[0] = make_unique<Counter>(scheduler, output0, time);
	counter[1] = make_unique<Counter>(scheduler, output1, time);
	counter[2] = make_unique<Counter>(scheduler, output2, time);
}

I8254::~I8254()
{
}

void I8254::reset(EmuTime::param time)
{
	for (auto& c : counter) {
		c->reset(time);
	}
}

byte I8254::readIO(word port, EmuTime::param time)
{
	port &= 3;
	switch (port) {
		case 0: case 1: case 2: // read counter 0, 1, 2
			return counter[port]->readIO(time);
		case 3: // read from control word, illegal
			return 255; // TODO check value
		default:
			UNREACHABLE; return 0;
	}
}

byte I8254::peekIO(word port, EmuTime::param time) const
{
	port &= 3;
	switch (port) {
		case 0: case 1: case 2:// read counter 0, 1, 2
			return counter[port]->peekIO(time);
		case 3: // read from control word, illegal
			return 255; // TODO check value
		default:
			UNREACHABLE; return 0;
	}
}

void I8254::writeIO(word port, byte value, EmuTime::param time)
{
	port &= 3;
	switch (port) {
		case 0: case 1: case 2: // write counter 0, 1, 2
			counter[port]->writeIO(value, time);
			break;
		case 3:
			// write to control register
			if ((value & READ_BACK) != READ_BACK) {
				// set control word of a counter
				counter[value >> 6]->writeControlWord(
				                           value & 0x3F, time);
			} else {
				// Read-Back-Command
				if (value & RB_CNTR0) {
					readBackHelper(value, 0, time);
				}
				if (value & RB_CNTR1) {
					readBackHelper(value, 1, time);
				}
				if (value & RB_CNTR2) {
					readBackHelper(value, 2, time);
				}
			}
			break;
		default:
			UNREACHABLE;
	}
}

void I8254::readBackHelper(byte value, unsigned cntr, EmuTime::param time)
{
	assert(cntr < 3);
	if (!(value & RB_STATUS)) {
		counter[cntr]->latchStatus(time);
	}
	if (!(value & RB_COUNT)) {
		counter[cntr]->latchCounter(time);
	}
}

void I8254::setGate(unsigned cntr, bool status, EmuTime::param time)
{
	assert(cntr < 3);
	counter[cntr]->setGateStatus(status, time);
}

ClockPin& I8254::getClockPin(unsigned cntr)
{
	assert(cntr < 3);
	return counter[cntr]->clock;
}

ClockPin& I8254::getOutputPin(unsigned cntr)
{
	assert(cntr < 3);
	return counter[cntr]->output;
}


// class Counter

Counter::Counter(Scheduler& scheduler, ClockPinListener* listener,
                        EmuTime::param time)
	: clock(scheduler), output(scheduler, listener)
	, currentTime(time)
{
	gate = true;
	counter = 0;
	counterLoad = 0;
	reset(time);
}

void Counter::reset(EmuTime::param time)
{
	currentTime = time;
	ltchCtrl = false;
	ltchCntr = false;
	readOrder  = LOW;
	writeOrder = LOW;
	control = 0x30; // Write BOTH / mode 0 / binary mode
	active = false;
	counting = true;
	triggered = false;

	// avoid UMR on savestate
	latchedCounter = 0;
	latchedControl = 0;
	writeLatch = 0;
}

byte Counter::readIO(EmuTime::param time)
{
	if (ltchCtrl) {
		ltchCtrl = false;
		return latchedControl;
	}
	advance(time);
	word readData = ltchCntr ? latchedCounter : counter;
	switch (control & WRT_FRMT) {
	case WF_LATCH:
		UNREACHABLE;
	case WF_LOW:
		ltchCntr = false;
		return readData & 0x00FF;
	case WF_HIGH:
		ltchCntr = false;
		return readData >> 8;
	case WF_BOTH:
		if (readOrder == LOW) {
			readOrder = HIGH;
			return readData & 0x00FF;
		} else {
			readOrder = LOW;
			ltchCntr = false;
			return readData >> 8;
		}
	default:
		UNREACHABLE; return 0;
	}
}

byte Counter::peekIO(EmuTime::param time) const
{
	if (ltchCtrl) {
		return latchedControl;
	}

	const_cast<Counter*>(this)->advance(time);

	word readData = ltchCntr ? latchedCounter : counter;
	switch (control & WRT_FRMT) {
	case WF_LATCH:
		UNREACHABLE;
	case WF_LOW:
		return readData & 0x00FF;
	case WF_HIGH:
		return readData >> 8;
	case WF_BOTH:
		if (readOrder == LOW) {
			return readData & 0x00FF;
		} else {
			return readData >> 8;
		}
	default:
		UNREACHABLE; return 0;
	}
}

void Counter::writeIO(byte value, EmuTime::param time)
{
	advance(time);
	switch (control & WRT_FRMT) {
	case WF_LATCH:
		UNREACHABLE;
	case WF_LOW:
		writeLoad((counterLoad & 0xFF00) | value, time);
		break;
	case WF_HIGH:
		writeLoad((counterLoad & 0x00FF) | (value << 8), time);
		break;
	case WF_BOTH:
		if (writeOrder == LOW) {
			writeOrder = HIGH;
			writeLatch = value;
			if ((control & CNTR_MODE) == CNTR_M0)
				// pauze counting when in mode 0
				counting = false;
		} else {
			writeOrder = LOW;
			counting = true;
			writeLoad((value << 8) | writeLatch, time);
		}
		break;
	default:
		UNREACHABLE;
	}
}
void Counter::writeLoad(word value, EmuTime::param time)
{
	counterLoad = value;
	byte mode = control & CNTR_MODE;
	if ((mode==CNTR_M0) || (mode==CNTR_M4)) {
		counter = counterLoad;
	}
	if (!active && ((mode == CNTR_M2) || (mode == CNTR_M2_) ||
	                 (mode == CNTR_M3) || (mode == CNTR_M3_))) {
		if (clock.isPeriodic()) {
			counter = counterLoad;
			EmuDuration::param high = clock.getTotalDuration();
			EmuDuration total = high * counter;
			output.setPeriodicState(total, high, time);
		} else {
			// TODO ??
		}
	}
	if (mode == CNTR_M0) {
		output.setState(false, time);
	}
	active = true; // counter is (re)armed after counter is initialized
}

void Counter::writeControlWord(byte value, EmuTime::param time)
{
	advance(time);
	if ((value & WRT_FRMT) == 0) {
		// counter latch command
		latchCounter(time);
		return;
	} else {
		// new control mode
		control = value;
		writeOrder = LOW;
		counting = true;
		active = false;
		triggered = false;
		switch (control & CNTR_MODE) {
		case CNTR_M0:
			output.setState(false, time);
			break;
		case CNTR_M1:
		case CNTR_M2: case CNTR_M2_:
		case CNTR_M3: case CNTR_M3_:
		case CNTR_M4:
		case CNTR_M5:
			output.setState(true, time);
			break;
		default:
			UNREACHABLE;
		}
	}
}

void Counter::latchStatus(EmuTime::param time)
{
	advance(time);
	if (!ltchCtrl) {
		ltchCtrl = true;
		byte out = output.getState(time) ? 0x80 : 0;
		latchedControl = out | control; // TODO bit 6 null-count
	}
}

void Counter::latchCounter(EmuTime::param time)
{
	advance(time);
	if (!ltchCntr) {
		ltchCntr = true;
		readOrder = LOW;
		latchedCounter = counter;
	}
}

void Counter::setGateStatus(bool newStatus, EmuTime::param time)
{
	advance(time);
	if (gate != newStatus) {
		gate = newStatus;
		switch (control & CNTR_MODE) {
		case CNTR_M0:
		case CNTR_M4:
			// Gate does not influence output, it just acts as a count enable
			// nothing needs to be done
			break;
		case CNTR_M1:
			if (gate && active) {
				// rising edge
				counter = counterLoad;
				output.setState(false, time);
				triggered = true;
			}
			break;
		case CNTR_M2: case CNTR_M2_:
		case CNTR_M3: case CNTR_M3_:
			if (gate) {
				if (clock.isPeriodic()) {
					counter = counterLoad;
					EmuDuration::param high = clock.getTotalDuration();
					EmuDuration total = high * counter;
					output.setPeriodicState(total, high, time);
				} else {
					// TODO ???
				}
			} else {
				output.setState(true, time);
			}
			break;
		case CNTR_M5:
			if (gate && active) {
				// rising edge
				counter = counterLoad;
				triggered = true;
			}
			break;
		default:
			UNREACHABLE;
		}
	}
}

void Counter::advance(EmuTime::param time)
{
	// TODO !!!! Set SP !!!!
	// TODO BCD counting
	int ticks = int(clock.getTicksBetween(currentTime, time));
	currentTime = time;
	switch (control & CNTR_MODE) {
	case CNTR_M0:
		if (gate && counting) {
			counter -= ticks;
			if (counter < 0) {
				counter &= 0xFFFF;
				if (active) {
					output.setState(false, time);
					active = false; // not periodic
				}
			}
		}
		break;
	case CNTR_M1:
		counter -= ticks;
		if (triggered) {
			if (counter < 0) {
				output.setState(true, time);
				triggered = false; // not periodic
			}
		}
		counter &= 0xFFFF;
		break;
	case CNTR_M2:
	case CNTR_M2_:
		if (gate) {
			counter -= ticks;
			if (active) {
				// TODO not completely correct
				if (counterLoad != 0) {
					counter %= counterLoad;
				} else {
					counter = 0;
				}
			}
		}
		break;
	case CNTR_M3:
	case CNTR_M3_:
		if (gate) {
			counter -= 2 * ticks;
			if (active) {
				// TODO not correct
				if (counterLoad != 0) {
					counter %= counterLoad;
				} else {
					counter = 0;
				}
			}
		}
		break;
	case CNTR_M4:
		if (gate) {
			counter -= ticks;
			if (active) {
				if (counter == 0) {
					output.setState(false, time);
				} else if (counter < 0) {
					output.setState(true, time);
					active = false; // not periodic
				}
			}
			counter &= 0xFFFF;
		}
		break;
	case CNTR_M5:
		counter -= ticks;
		if (triggered) {
			if (counter == 0)
				output.setState(false, time);
			if (counter < 0) {
				output.setState(true, time);
				triggered = false; // not periodic
			}
		}
		counter &= 0xFFFF;
		break;
	default:
		UNREACHABLE;
	}
}


static std::initializer_list<enum_string<Counter::ByteOrder>> byteOrderInfo = {
	{ "LOW",  Counter::LOW  },
	{ "HIGH", Counter::HIGH }
};
SERIALIZE_ENUM(Counter::ByteOrder, byteOrderInfo);

template<typename Archive>
void Counter::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("clock", clock);
	ar.serialize("output", output);
	ar.serialize("currentTime", currentTime);
	ar.serialize("counter", counter);
	ar.serialize("latchedCounter", latchedCounter);
	ar.serialize("counterLoad", counterLoad);
	ar.serialize("control", control);
	ar.serialize("latchedControl", latchedControl);
	ar.serialize("ltchCtrl", ltchCtrl);
	ar.serialize("ltchCntr", ltchCntr);
	ar.serialize("readOrder", readOrder);
	ar.serialize("writeOrder", writeOrder);
	ar.serialize("writeLatch", writeLatch);
	ar.serialize("gate", gate);
	ar.serialize("active", active);
	ar.serialize("triggered", triggered);
	ar.serialize("counting", counting);
}

template<typename Archive>
void I8254::serialize(Archive& ar, unsigned /*version*/)
{
	char tag[9] = { 'c', 'o', 'u', 'n', 't', 'e', 'r', 'X', 0 };
	for (int i = 0; i < 3; ++i) {
		tag[7] = char('0' + i);
		ar.serialize(tag, *counter[i]);
	}
}
INSTANTIATE_SERIALIZE_METHODS(I8254);

} // namespace openmsx
