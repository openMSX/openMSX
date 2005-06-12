// $Id$

#include <cassert>
#include "I8251.hh"
#include "Scheduler.hh"

using std::string;

namespace openmsx {

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
const byte MODE_RATE1       = 0x01;
const byte MODE_RATE16      = 0x02;
const byte MODE_RATE64      = 0x03;
const byte MODE_WORDLENGTH  = 0x0C;
const byte MODE_5BIT        = 0x00;
const byte MODE_6BIT        = 0x04;
const byte MODE_7BIT        = 0x08;
const byte MODE_8BIT        = 0x0C;
const byte MODE_PARITYEN    = 0x10;
const byte MODE_PARITODD    = 0x00;
const byte MODE_PARITEVEN   = 0x20;
const byte MODE_STOP_BITS   = 0xC0;
const byte MODE_STOP_INV    = 0x00;
const byte MODE_STOP_1      = 0x40;
const byte MODE_STOP_15     = 0x80;
const byte MODE_STOP_2      = 0xC0;
const byte MODE_SINGLESYNC  = 0x80;

const byte CMD_TXEN   = 0x01;
const byte CMD_DTR    = 0x02;
const byte CMD_RXE    = 0x04;
const byte CMD_SBRK   = 0x08;
const byte CMD_RSTERR = 0x10;
const byte CMD_RTS    = 0x20;
const byte CMD_RESET  = 0x40;
const byte CMD_HUNT   = 0x80;

enum SyncPointType {
	RECV, TRANS
};


I8251::I8251(I8251Interface* interf_, const EmuTime& time)
	: interf(interf_), recvBuf(0)
{
	reset(time);
}

I8251::~I8251()
{
	removeSyncPoint(TRANS);
	removeSyncPoint(RECV);
}

void I8251::reset(const EmuTime& time)
{
	status = STAT_TXRDY | STAT_TXEMPTY;
	command = 0xFF; // make sure all bits change
	writeCommand(0, time);
	cmdFaze = FAZE_MODE;
}

byte I8251::readIO(byte port, const EmuTime& time)
{
	byte result;
	switch (port & 1) {
	case 0:
		result = readTrans(time);
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

byte I8251::peekIO(byte port, const EmuTime& /*time*/) const
{
	byte result;
	switch (port & 1) {
	case 0:
		result = recvBuf;
		break;
	case 1:
		result = status; // TODO peekStatus()
		break;
	default:
		assert(false);
		result = 0xFF;
	}
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
			setMode(value);
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

void I8251::setMode(byte value)
{
	mode = value;

	SerialDataInterface::DataBits dataBits;
	switch (mode & MODE_WORDLENGTH) {
	case MODE_5BIT:
		dataBits = SerialDataInterface::DATA_5;
		break;
	case MODE_6BIT:
		dataBits = SerialDataInterface::DATA_6;
		break;
	case MODE_7BIT:
		dataBits = SerialDataInterface::DATA_7;
		break;
	case MODE_8BIT:
		dataBits = SerialDataInterface::DATA_8;
		break;
	default:
		assert(false);
		dataBits = SerialDataInterface::DATA_8;
	}
	interf->setDataBits(dataBits);

	SerialDataInterface::StopBits stopBits;
	switch(mode & MODE_STOP_BITS) {
	case MODE_STOP_INV:
		stopBits = SerialDataInterface::STOP_INV;
		break;
	case MODE_STOP_1:
		stopBits = SerialDataInterface::STOP_1;
		break;
	case MODE_STOP_15:
		stopBits = SerialDataInterface::STOP_15;
		break;
	case MODE_STOP_2:
		stopBits = SerialDataInterface::STOP_2;
		break;
	default:
		assert(false);
		stopBits = SerialDataInterface::STOP_2;
	}
	interf->setStopBits(stopBits);

	bool parityEnable = mode & MODE_PARITYEN;
	SerialDataInterface::ParityBit parity = (mode & MODE_PARITEVEN) ?
		SerialDataInterface::EVEN : SerialDataInterface::ODD;
	interf->setParityBit(parityEnable, parity);

	int baudrate;
	switch (mode & MODE_BAUDRATE) {
	case MODE_SYNCHRONOUS:
		baudrate = 1;
		break;
	case MODE_RATE1:
		baudrate = 1;
		break;
	case MODE_RATE16:
		baudrate = 16;
		break;
	case MODE_RATE64:
		baudrate = 64;
		break;
	default:
		assert(false);
		baudrate = 1;
	}

	charLength = (((2 * (1 + (int)dataBits + (parityEnable ? 1 : 0))) +
	               (int)stopBits) * baudrate) / 2;
}

void I8251::writeCommand(byte value, const EmuTime& time)
{
	byte oldCommand = command;
	command = value;

	// CMD_RESET, CMD_TXEN, CMD_RXE  handled in other routines

	interf->setRTS(command & CMD_RTS, time);
	interf->setDTR(command & CMD_DTR, time);

	if (!(command & CMD_TXEN)) {
		// disable transmitter
		removeSyncPoint(TRANS);
		status |= STAT_TXRDY | STAT_TXEMPTY;
	}
	if (command & CMD_RSTERR) {
		status &= ~(STAT_PE | STAT_OE | STAT_FE);
	}
	if (command & CMD_SBRK) {
		// TODO
	}
	if (command & CMD_HUNT) {
		// TODO
	}

	if ((command ^ oldCommand) & CMD_RXE) {
		if (command & CMD_RXE) {
			// enable receiver
			status &= ~(STAT_PE | STAT_OE | STAT_FE); // TODO
			recvReady = true;
		} else {
			// disable receiver
			removeSyncPoint(RECV);
			status &= ~(STAT_PE | STAT_OE | STAT_FE); // TODO
			status &= ~STAT_RXRDY;
		}
		interf->signal(time);
	}
}

byte I8251::readStatus(const EmuTime& time)
{
	byte result = status;
	if (interf->getDSR(time)) {
		result |= STAT_DSR;
	}
	return result;
}

byte I8251::readTrans(const EmuTime& time)
{
	status &= ~STAT_RXRDY;
	interf->setRxRDY(false, time);
	return recvBuf;
}

void I8251::writeTrans(byte value, const EmuTime& time)
{
	if (!(command & CMD_TXEN)) {
		return;
	}
	if (status & STAT_TXEMPTY) {
		// not sending
		send(value, time);
	} else {
		sendBuffer = value;
		status &= ~STAT_TXRDY;
	}
}

ClockPin& I8251::getClockPin()
{
	return clock;
}


void I8251::setDataBits(DataBits bits)
{
	recvDataBits = bits;
}

void I8251::setStopBits(StopBits bits)
{
	recvStopBits = bits;
}

void I8251::setParityBit(bool enable, ParityBit parity)
{
	recvParityEnabled = enable;
	recvParityBit = parity;
}

void I8251::recvByte(byte value, const EmuTime& time)
{
	// TODO STAT_PE / STAT_FE / STAT_SYNBRK
	assert(recvReady && (command & CMD_RXE));
	if (status & STAT_RXRDY) {
		status |= STAT_OE;
	} else {
		recvBuf = value;
		status |= STAT_RXRDY;
		interf->setRxRDY(true, time);
	}
	recvReady = false;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		setSyncPoint(next, RECV);
	}
}

bool I8251::isRecvReady()
{
	return recvReady;
}
bool I8251::isRecvEnabled()
{
	return command & CMD_RXE;
}

void I8251::send(byte value, const EmuTime& time)
{
	status &= ~STAT_TXEMPTY;
	sendByte = value;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		setSyncPoint(next, TRANS);
	}
}

void I8251::executeUntil(const EmuTime &time, int userData)
{
	switch ((SyncPointType)userData) {
	case RECV:
		assert(command & CMD_RXE);
		recvReady = true;
		interf->signal(time);
		break;
	case TRANS:
		assert(!(status & STAT_TXEMPTY) && (command & CMD_TXEN));

		interf->recvByte(sendByte, time);
		if (status & STAT_TXRDY) {
			status |= STAT_TXEMPTY;
		} else {
			status |= STAT_TXRDY;
			send(sendBuffer, time);
		}
		break;
	default:
		assert(false);
	}
}

const string& I8251::schedName() const
{
	static const string I8251_NAME("I8251");
	return I8251_NAME;
}

} // namespace openmsx
