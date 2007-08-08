// $Id$

// TODO: actually port it, see all the TODO's! :)

#include "YM2148.hh"

namespace openmsx {

static const unsigned STAT_RXRDY   = 0x02;
static const unsigned STAT_TXEMPTY = 0x01;
static const unsigned STAT_PE      = 0x10;
static const unsigned STAT_OE      = 0x20; //???  MR checks 0x30
static const unsigned ST_INT       = 0x800;

static const unsigned CMD_RDINT = 0x08;
static const unsigned CMD_RSTER = 0x10;
static const unsigned CMD_WRINT = 0x100;
static const unsigned CMD_RST   = 0x80;

void YM2148::midiInCallback(byte* buffer, unsigned length)
{
	// TODO: archSemaphoreWait(semaphore, -1);
	if ((rxPending + length) < RX_QUEUE_SIZE) {
		while (length--) {
			rxQueue[rxHead & (RX_QUEUE_SIZE - 1)] = *buffer++;
			rxHead++;
			rxPending++;
		}
	}
	//TODO: archSemaphoreSignal(semaphore);
}

void YM2148::onRecv()
{
	timeRecv = 0;

	if (status & STAT_RXRDY) {
		status |= STAT_OE;
		if (command & CMD_RSTER) {
			reset();
			return;
		}
	} 

	if (rxPending != 0) {
		// TODO: archSemaphoreWait(semaphore, -1);
		rxData = rxQueue[(rxHead - rxPending) & (RX_QUEUE_SIZE - 1)];
		rxPending--;
		// TODO: archSemaphoreSignal(semaphore);
		status |= STAT_RXRDY;
		if (command & CMD_RDINT) {
			// TODO: boardSetDataBus(vector, 0, 0);
			// TODO: boardSetInt(0x800);
			status |= ST_INT;
		}
	}
	/* TODO:
	   timeRecv = boardSystemTime() + charTime;
	   boardTimerAdd(timerRecv, timeRecv);
	   */
}

void YM2148::onTrans()
{
	timeTrans = 0;

	if (status & STAT_TXEMPTY) {
		txPending = 0;
	} else {
		// TODO: midiIoTransmit(midiIo, txBuffer);
		// TODO: timeTrans = boardSystemTime() + charTime;
		// TODO: boardTimerAdd(timerTrans, timeTrans);

		status |= STAT_TXEMPTY;
		if (command & CMD_WRINT) {
			/* TODO:
			boardSetDataBus(vector, 0, 0);
			boardSetInt(0x800);*/
			status |= ST_INT;
		}
	}
}

void YM2148::reset()
{
	status = STAT_TXEMPTY;
	txPending = 0;
	rxPending = 0;
	command = 0;
	timeRecv = 0;
	timeTrans = 0;
	// TODO: charTime = 10 * boardFrequency() / 31250;

	
	// TODO: boardTimerRemove(timerRecv);
	// TODO: boardTimerRemove(timerTrans);

	// TODO:timeRecv = boardSystemTime() + charTime;
	// TODO: boardTimerAdd(timerRecv, timeRecv);
}

YM2148::YM2148()
{
	/* TODO:
	midiIo = midiIoCreate(midiInCallback, this);
	semaphore = archSemaphoreCreate(1);
	timerRecv   = boardTimerCreate(onRecv, this);
	timerTrans  = boardTimerCreate(onTrans, this);

	timeRecv = boardSystemTime() + charTime;
	boardTimerAdd(timerRecv, timeRecv);*/
}

YM2148::~YM2148()
{
	/* TODO:
	midiIoDestroy(midiIo);
	archSemaphoreDestroy(semaphore);
	*/
}

byte YM2148::readStatus()
{
	byte val = status;

	// TODO: boardClearInt(0x800);
	status &= ~ST_INT;

	return val;
}

byte YM2148::readData()
{
	status &= ~(STAT_RXRDY | STAT_OE);
	return rxData;
}

//TODO: I'm not sure if this makes sense
void YM2148::setVector(byte value)
{
	vector = value;
	// TODO: boardSetDataBus(vector, 0, 0);
}

void YM2148::writeCommand(byte value)
{
	command = value;

	if (value & 0x02) {
		// ??? What is this?
	}

	if (value & CMD_RST) {
		reset();
	}

	// TODO: charTime = (unsigned)((uint64)144 * boardFrequency() / 500000);
}

void YM2148::writeData(byte value)
{
	if (!(status & STAT_TXEMPTY)) {
		return;
	}

	if (txPending == 0) {
		// TODO: midiIoTransmit(midiIo, value);
		// TODO: timeTrans = boardSystemTime() + charTime;
		// TODO: boardTimerAdd(timerTrans, timeTrans);
		txPending = 1;
	} else {
		status &= ~STAT_TXEMPTY;
		txBuffer = value;
	}
}

} // namespace openmsx

