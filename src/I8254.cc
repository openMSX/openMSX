// $Id$

#include <cassert>
#include "I8254.hh"


I8254::I8254(int freq[NR_COUNTERS], I8254Interface *interf)
{
	interface = interf;
	for (int i=0; i<NR_COUNTERS; i++) {
		counter[i] = new Counter(freq[i], this, i);
	}
}

I8254::~I8254()
{
	for (int i=0; i<NR_COUNTERS; i++) {
		delete counter[i];
	}
}

void I8254::reset()
{
	for (int i=0; i<NR_COUNTERS; i++) {
		counter[i]->reset();
	}
}

byte I8254::readIO(byte port, const EmuTime &time)
{
	assert(port<NR_COUNTERS+1);
	if (port<NR_COUNTERS) {
		// read from counters
		return counter[port]->readIO(time);
	} else {
		// read from control word
		// illegal
		return 255;	//TODO check value
	}
}

void I8254::writeIO(byte port, byte value, const EmuTime &time)
{
	assert(port<NR_COUNTERS+1);
	if (port<NR_COUNTERS) {
		// write to one of the counters
		counter[port]->writeIO(value, time);
	} else {
		// write to control register
		if ((value&READ_BACK)!=READ_BACK) {
			// set control word of a counter
			counter[value>>6]->writeControlWord(value&0x3f, time);
		} else {
			// Read-Back-Command
			if (value&RB_CNTR0) readBackHelper(value, 0, time);
			if (value&RB_CNTR1) readBackHelper(value, 1, time);
			if (value&RB_CNTR2) readBackHelper(value, 2, time);
		}
	}
}
void I8254::readBackHelper(byte value, byte cntr, const EmuTime &time) {
	if (!(value&RB_STATUS)) counter[cntr]->latchStatus (time);
	if (!(value&RB_COUNT))  counter[cntr]->latchCounter(time);
}

void I8254::setGateStatus(byte cntr, bool status, const EmuTime &time)
{
	assert(cntr<NR_COUNTERS);
	counter[cntr]->setGateStatus(status, time);
}

bool I8254::getOutput(byte cntr, const EmuTime &time)
{
	assert(cntr<NR_COUNTERS);
	return counter[cntr]->getOutput(time);
}

void I8254::outputCallback(int id, bool status)
{
	     if (id==0) interface->output0(status);
	else if (id==1) interface->output1(status);
	else if (id==2) interface->output2(status);
	assert(false);
}


/////


I8254::Counter::Counter(int freq, I8254 *prnt, int _id) : currentTime(freq)
{
	parent = prnt;
	id = _id;
	reset();
}

void I8254::Counter::reset()
{
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
		return readData&0x00ff;
	case WF_HIGH:
		ltchCntr = false;
		return readData>>8;
	case WF_BOTH:
		if (readOrder==LOW) {
			readOrder = HIGH;
			return readData&0x00ff;
		} else {
			readOrder = LOW;
			ltchCntr = false;
			return readData>>8;
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
		writeLoad((counterLoad&0xff00)|value);
		break;
	case WF_HIGH:
		writeLoad((counterLoad&0x00ff)|(value<<8));
		break;
	case WF_BOTH:
		if (writeOrder==LOW) {
			writeOrder = HIGH;
			writeLatch = value;
			if ((control & CNTR_MODE) == CNTR_M0)
				// pauze counting when in mode 0
				counting = false;
		} else {
			writeOrder = LOW;
			counting = true;
			writeLoad((value<<8)|writeLatch);
		}
		break;
	default:
		assert(false);
	}
}
void I8254::Counter::writeLoad(word value) 
{
	counterLoad = value;
	byte mode = control & CNTR_MODE;
	if ( ((mode==CNTR_M0) || (mode==CNTR_M4)) ||
	     (!active && (mode==CNTR_M2||mode==CNTR_M2_||mode==CNTR_M3||mode==CNTR_M3_)) )
		counter = counterLoad;
	if (mode==CNTR_M0)
		changeOutput(false);
	active = true;	// counter is (re)armed after counter is initialized
}

bool I8254::Counter::getOutput(const EmuTime &time)
{
	advance(time);
	return output;
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
		counting = true; active = false; triggered = false;
		switch (control & CNTR_MODE) {
		case CNTR_M0:
			changeOutput(false);
			break;
		case CNTR_M1:
		case CNTR_M2: case CNTR_M2_:
		case CNTR_M3: case CNTR_M3_:
		case CNTR_M4:
		case CNTR_M5:
			changeOutput(true);
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
		byte out = output ? 0x80 : 0;
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
			if (gate && active) {	// rising edge
				counter = counterLoad;
				changeOutput(false);
				triggered = true;
			}
			break;
		case CNTR_M2: case CNTR_M2_:
		case CNTR_M3: case CNTR_M3_:
			if (gate) {
				counter = counterLoad;
			} else {
				changeOutput(true);
			}
			break;
		case CNTR_M5:
			if (gate & active) {	//rising edge
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
	uint64 ticks = currentTime.getTicksTill(time);
	currentTime = time;
	switch (control & CNTR_MODE) {
	case CNTR_M0:
		if (gate && counting) {
			counter -= ticks;
			if (counter<0) {
				counter &= 0xffff;
				if (active) {
					changeOutput(false);
					active = false;	// not periodic
				}
			}
		}
		break;
	case CNTR_M1:
		counter -= ticks;
		if (triggered) {
			if (counter<0) {
				changeOutput(true);
				triggered = false;	// not periodic
			}
		}
		counter &= 0xffff;
		break;
	case CNTR_M2: case CNTR_M2_:
		if (gate) {
			counter -= ticks;
			if (active) {
				if (counter == 1)
					changeOutput(false);
				if (counter <= 0) {
					counter = counterLoad;
					changeOutput(true);
				}
			}
		}
		break;
	case CNTR_M3: case CNTR_M3_:
		if (gate) {
			counter -= 2*ticks;
			if (active) {
				if (counter <= 0) {
					if (output)
						counter = counterLoad & ~1;
					else
						counter = counterLoad;
					changeOutput(!output);
				}
			}
		}
		break;
	case CNTR_M4:
		if (gate) {
			counter -= ticks;
			if (active) {
				if (counter == 0)
					changeOutput(false);
				if (counter < 0) {
					changeOutput(true);
					active = false;	// not periodic
				}
			}
			counter &= 0xffff;
		}
		break;
	case CNTR_M5:
		counter -= ticks;
		if (triggered) {
			if (counter == 0)
				changeOutput(false);
			if (counter < 0) {
				changeOutput(true);
				triggered = false;	//not periodic
			}
		}
		counter &= 0xffff;
		break;
	default:
		assert(false);
	}
}

void I8254::Counter::changeOutput(bool newOutput)
{
	if (newOutput != output) {
		output = newOutput;
		parent->outputCallback(id, output);
	}
}
	
