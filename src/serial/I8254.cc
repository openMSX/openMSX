// $Id$

#include <cassert>
#include "I8254.hh"


/// class I8254 ///

I8254::I8254(ClockPinListener* output0, ClockPinListener* output1,
             ClockPinListener* output2, const EmuTime &time)
	: counter0(output0, time),
	  counter1(output1, time),
	  counter2(output2, time)
{
}

I8254::~I8254()
{
}

void I8254::reset(const EmuTime &time)
{
	counter0.reset(time);
	counter1.reset(time);
	counter2.reset(time);
}

byte I8254::readIO(byte port, const EmuTime &time)
{
	port &= 3;
	switch (port) {
		case 0: // read counter 0
			return counter0.readIO(time);
		case 1: // read counter 1
			return counter1.readIO(time);
		case 2: // read counter 2
			return counter2.readIO(time);
		case 3: // read from control word, illegal
			return 255;	//TODO check value
		default:
			assert(false);
			return 255;
	}
}

void I8254::writeIO(byte port, byte value, const EmuTime &time)
{
	port &= 3;
	switch (port) {
		case 0: // write counter 0
			counter0.writeIO(value, time);
		case 1: // write counter 1
			counter1.writeIO(value, time);
		case 2: // write counter 2
			counter2.writeIO(value, time);
			break;
		case 3:
			// write to control register
			if ((value & READ_BACK) != READ_BACK) {
				// set control word of a counter
				getCounter(value >> 6).writeControlWord(
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
			assert(false);
	}
}

void I8254::readBackHelper(byte value, byte cntr, const EmuTime &time)
{
	Counter &c = getCounter(cntr);
	if (!(value & RB_STATUS)) {
		c.latchStatus(time);
	}
	if (!(value & RB_COUNT)) {
		c.latchCounter(time);
	}
}

void I8254::setGate(byte cntr, bool status, const EmuTime &time)
{
	getCounter(cntr).setGateStatus(status, time);
}

ClockPin& I8254::getClockPin(byte cntr)
{
	return getCounter(cntr).clock;
}

ClockPin& I8254::getOutputPin(byte cntr)
{
	return getCounter(cntr).output;
}

I8254::Counter& I8254::getCounter(byte cntr)
{
	switch (cntr) {
		case 0: return counter0;
		case 1: return counter1;
		case 2: return counter2;
		default: assert(false); return counter0;
	}
}

/// class Counter ///

I8254::Counter::Counter(ClockPinListener* listener, const EmuTime &time)
	: output(listener)
{
	gate = true;
	counter = 0;
	counterLoad = 0;
	reset(time);
}

void I8254::Counter::reset(const EmuTime &time)
{
	currentTime = time;
	ltchCtrl = ltchCntr = false;
	readOrder = writeOrder = LOW;
	control = 0x30;	// Write BOTH / mode 0 / binary mode
	active = false;
	counting = true;
}

byte I8254::Counter::readIO(const EmuTime &time)
{
	if (ltchCtrl) {
		ltchCtrl = false;
		return latchedControl;
	}
	advance(time);
	word readData = ltchCntr ? latchedCounter : counter;
	switch (control & WRT_FRMT) {
	case WF_LATCH:
		assert(false);
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
		assert(false);
		return 0;	// avoid warning
	}
}

void I8254::Counter::writeIO(byte value, const EmuTime &time)
{
	advance(time);
	switch (control & WRT_FRMT) {
	case WF_LATCH:
		assert(false);
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
		assert(false);
	}
}
void I8254::Counter::writeLoad(word value, const EmuTime& time)
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
			const EmuDuration& high = clock.getTotalDuration();
			EmuDuration total = high * counter;
			output.setPeriodicState(total, high, time);
		} else {
			// TODO ??
		}
	}
	if (mode == CNTR_M0) {
		output.setState(false, time);
	}
	active = true;	// counter is (re)armed after counter is initialized
}

void I8254::Counter::writeControlWord(byte value, const EmuTime &time)
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
			assert(false);
		}
	}
}

void I8254::Counter::latchStatus(const EmuTime &time)
{
	advance(time);
	if (!ltchCtrl) {
		ltchCtrl = true;
		byte out = output.getState(time) ? 0x80 : 0;
		latchedControl = out | control; // TODO bit 6 null-count
	}
}

void I8254::Counter::latchCounter(const EmuTime &time)
{
	advance(time);
	if (!ltchCntr) {
		ltchCntr = true;
		readOrder = LOW;
		latchedCounter = counter;
	}
}

void I8254::Counter::setGateStatus(bool newStatus, const EmuTime &time)
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
					const EmuDuration& high = clock.getTotalDuration();
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
			if (gate & active) {
				// rising edge
				counter = counterLoad;
				triggered = true;
			}
			break;
		default:
			assert(false);
		}
	}
}

void I8254::Counter::advance(const EmuTime &time)
{
	//TODO !!!! Set SP !!!!
	//TODO BCD counting
	uint64 ticks = clock.getTicksBetween(currentTime, time);
	currentTime = time;
	switch (control & CNTR_MODE) {
	case CNTR_M0:
		if (gate && counting) {
			counter -= ticks;
			if (counter < 0) {
				counter &= 0xFFFF;
				if (active) {
					output.setState(false, time);
					active = false;	// not periodic
				}
			}
		}
		break;
	case CNTR_M1:
		counter -= ticks;
		if (triggered) {
			if (counter < 0) {
				output.setState(true, time);
				triggered = false;	// not periodic
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
					active = false;	// not periodic
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
				triggered = false;	//not periodic
			}
		}
		counter &= 0xFFFF;
		break;
	default:
		assert(false);
	}
}
