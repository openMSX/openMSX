// $Id$

#include <cassert>
#include "I8251.hh"


const byte STAT_TXRDY   = 0x01;
const byte STAT_RXRDY   = 0x02;
const byte STAT_TXEMPTY = 0x04;
const byte STAT_PE      = 0x08;
const byte STAT_OE      = 0x10;
const byte STAT_FE      = 0x20;
const byte STAT_SYNBRK  = 0x40;
const byte STAT_DSR     = 0x80;

const byte MODE_BAUDRATE    = 0x03;
const byte MODE_SYNCHRONOUS = 0x00;
const byte MODE_SINGLESYNC  = 0x80;

const byte CMD_DTR   = 0x02;
const byte CMD_RTS   = 0x20;
const byte CMD_RESET = 0x40;


I8251::I8251(I8251Interface* interf_, const EmuTime& time)
	: interf(interf_)
{
	status = STAT_TXRDY | STAT_RXRDY | STAT_TXEMPTY;
	reset(time);
}

I8251::~I8251()
{
}

void I8251::reset(const EmuTime& time)
{
	//writeCommand(0, time);
	cmdFaze = FAZE_MODE;
}

byte I8251::readIO(byte port, const EmuTime& time)
{
	byte result;
	switch (port & 1) {
	case 0:
		result = readRecv(time);
		break;
	case 1:
		result = readStatus(time);
		break;
	default:
		assert(false);
		result = 0xFF;
	}
	//PRT_DEBUG("I8251: read " << (int)port << " " << (int)result);
	return result;
}

void I8251::writeIO(byte port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("I8251: write " << (int)port << " " << (int)value);
	switch (port & 1) {
	case 0:
		writeTrans(value, time);
		break;
	case 1:
		switch (cmdFaze) {
		case FAZE_MODE:
			mode = value;
			if ((mode & MODE_BAUDRATE) == MODE_SYNCHRONOUS) {
				cmdFaze = FAZE_SYNC1;
			} else {
				cmdFaze = FAZE_CMD;
			}
			break;
		case FAZE_SYNC1:
			sync1 = value;
			if (mode & MODE_SINGLESYNC) {
				cmdFaze = FAZE_CMD;
			} else {
				cmdFaze = FAZE_SYNC2;
			}
			break;
		case FAZE_SYNC2:
			sync2 = value;
			cmdFaze = FAZE_CMD;
			break;
		case FAZE_CMD:
			if (value & CMD_RESET) {
				cmdFaze = FAZE_MODE;
			} else {
				writeCommand(value, time);
			}
			break;
		default:
			assert(false);
		}
		break;
	default:
		assert(false);
	}
}

void I8251::writeCommand(byte value, const EmuTime& time)
{
	interf->setRTS(value & CMD_RTS, time);
	interf->setDTR(value & CMD_DTR, time);

	// TODO 
}

byte I8251::readStatus(const EmuTime& time)
{
	byte result = status;
	if (interf->getDSR(time)) {
		result |= STAT_DSR;
	}
	return result;
}

byte I8251::readRecv(const EmuTime& time)
{
	// TODO
	return 0;
}

void I8251::writeTrans(byte value, const EmuTime& time)
{
	// TODO
}

ClockPin& I8251::getClockPin()
{
	return clock;
}
