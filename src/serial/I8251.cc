#include "I8251.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <cassert>

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


I8251::I8251(Scheduler& scheduler, I8251Interface& interf_, EmuTime::param time)
	: syncRecv (scheduler)
	, syncTrans(scheduler)
	, interf(interf_), clock(scheduler)
{
	reset(time);
}

void I8251::reset(EmuTime::param time)
{
	// initialize these to avoid UMR on savestate
	//   TODO investigate correct initial state after reset
	charLength = 0;
	recvDataBits  = SerialDataInterface::DATA_8;
	recvStopBits  = SerialDataInterface::STOP_1;
	recvParityBit = SerialDataInterface::EVEN;
	recvParityEnabled = false;
	recvBuf = 0;
	recvReady = false;
	sendByte = 0;
	sendBuffer = 0;
	mode = 0;
	sync1 = sync2 = 0;

	status = STAT_TXRDY | STAT_TXEMPTY;
	command = 0xFF; // make sure all bits change
	writeCommand(0, time);
	cmdFaze = FAZE_MODE;
}

byte I8251::readIO(word port, EmuTime::param time)
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
		UNREACHABLE; return 0;
	}
	return result;
}

byte I8251::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 1) {
	case 0:
		return recvBuf;
	case 1:
		return status; // TODO peekStatus()
	default:
		UNREACHABLE; return 0;
	}
}


void I8251::writeIO(word port, byte value, EmuTime::param time)
{
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
			UNREACHABLE;
		}
		break;
	default:
		UNREACHABLE;
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
		UNREACHABLE;
		dataBits = SerialDataInterface::DATA_8;
	}
	interf.setDataBits(dataBits);

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
		UNREACHABLE;
		stopBits = SerialDataInterface::STOP_2;
	}
	interf.setStopBits(stopBits);

	bool parityEnable = (mode & MODE_PARITYEN) != 0;
	SerialDataInterface::ParityBit parity = (mode & MODE_PARITEVEN) ?
		SerialDataInterface::EVEN : SerialDataInterface::ODD;
	interf.setParityBit(parityEnable, parity);

	unsigned baudrate;
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
		UNREACHABLE;
		baudrate = 1;
	}

	charLength = (((2 * (1 + unsigned(dataBits) + (parityEnable ? 1 : 0))) +
	               unsigned(stopBits)) * baudrate) / 2;
}

void I8251::writeCommand(byte value, EmuTime::param time)
{
	byte oldCommand = command;
	command = value;

	// CMD_RESET, CMD_TXEN, CMD_RXE  handled in other routines

	interf.setRTS((command & CMD_RTS) != 0, time);
	interf.setDTR((command & CMD_DTR) != 0, time);

	if (!(command & CMD_TXEN)) {
		// disable transmitter
		syncTrans.removeSyncPoint();
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
			syncRecv.removeSyncPoint();
			status &= ~(STAT_PE | STAT_OE | STAT_FE); // TODO
			status &= ~STAT_RXRDY;
		}
		interf.signal(time);
	}
}

byte I8251::readStatus(EmuTime::param time)
{
	byte result = status;
	if (interf.getDSR(time)) {
		result |= STAT_DSR;
	}
	return result;
}

byte I8251::readTrans(EmuTime::param time)
{
	status &= ~STAT_RXRDY;
	interf.setRxRDY(false, time);
	return recvBuf;
}

void I8251::writeTrans(byte value, EmuTime::param time)
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

void I8251::setParityBit(bool enable, ParityBit parity)
{
	recvParityEnabled = enable;
	recvParityBit = parity;
}

void I8251::recvByte(byte value, EmuTime::param time)
{
	// TODO STAT_PE / STAT_FE / STAT_SYNBRK
	assert(recvReady && (command & CMD_RXE));
	if (status & STAT_RXRDY) {
		status |= STAT_OE;
	} else {
		recvBuf = value;
		status |= STAT_RXRDY;
		interf.setRxRDY(true, time);
	}
	recvReady = false;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		syncRecv.setSyncPoint(next);
	}
}

bool I8251::isRecvEnabled()
{
	return (command & CMD_RXE) != 0;
}

void I8251::send(byte value, EmuTime::param time)
{
	status &= ~STAT_TXEMPTY;
	sendByte = value;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		syncTrans.setSyncPoint(next);
	}
}

void I8251::execRecv(EmuTime::param time)
{
	assert(command & CMD_RXE);
	recvReady = true;
	interf.signal(time);
}

void I8251::execTrans(EmuTime::param time)
{
	assert(!(status & STAT_TXEMPTY) && (command & CMD_TXEN));

	interf.recvByte(sendByte, time);
	if (status & STAT_TXRDY) {
		status |= STAT_TXEMPTY;
	} else {
		status |= STAT_TXRDY;
		send(sendBuffer, time);
	}
}


static std::initializer_list<enum_string<SerialDataInterface::DataBits>> dataBitsInfo = {
		{ "5", SerialDataInterface::DATA_5 },
		{ "6", SerialDataInterface::DATA_6 },
		{ "7", SerialDataInterface::DATA_7 },
		{ "8", SerialDataInterface::DATA_8 }
};
SERIALIZE_ENUM(SerialDataInterface::DataBits, dataBitsInfo);

static std::initializer_list<enum_string<SerialDataInterface::StopBits>> stopBitsInfo = {
	{ "INVALID", SerialDataInterface::STOP_INV },
	{ "1",       SerialDataInterface::STOP_1   },
	{ "1.5",     SerialDataInterface::STOP_15  },
	{ "2",       SerialDataInterface::STOP_2   }
};
SERIALIZE_ENUM(SerialDataInterface::StopBits, stopBitsInfo);

static std::initializer_list<enum_string<SerialDataInterface::ParityBit>> parityBitInfo = {
	{ "EVEN", SerialDataInterface::EVEN },
	{ "ODD",  SerialDataInterface::ODD  }
};
SERIALIZE_ENUM(SerialDataInterface::ParityBit, parityBitInfo);

static std::initializer_list<enum_string<I8251::CmdFaze>> cmdFazeInfo = {
	{ "MODE",  I8251::FAZE_MODE  },
	{ "SYNC1", I8251::FAZE_SYNC1 },
	{ "SYNC2", I8251::FAZE_SYNC2 },
	{ "CMD",   I8251::FAZE_CMD   }
};
SERIALIZE_ENUM(I8251::CmdFaze, cmdFazeInfo);

// version 1: initial version
// version 2: removed 'userData' from Schedulable
template<typename Archive>
void I8251::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("syncRecv",  syncRecv);
		ar.serialize("syncTrans", syncTrans);
	} else {
		Schedulable::restoreOld(ar, {&syncRecv, &syncTrans});
	}
	ar.serialize("clock", clock);
	ar.serialize("charLength", charLength);
	ar.serialize("recvDataBits", recvDataBits);
	ar.serialize("recvStopBits", recvStopBits);
	ar.serialize("recvParityBit", recvParityBit);
	ar.serialize("recvParityEnabled", recvParityEnabled);
	ar.serialize("recvBuf", recvBuf);
	ar.serialize("recvReady", recvReady);
	ar.serialize("sendByte", sendByte);
	ar.serialize("sendBuffer", sendBuffer);
	ar.serialize("status", status);
	ar.serialize("command", command);
	ar.serialize("mode", mode);
	ar.serialize("sync1", sync1);
	ar.serialize("sync2", sync2);
	ar.serialize("cmdFaze", cmdFaze);
}
INSTANTIATE_SERIALIZE_METHODS(I8251);

} // namespace openmsx
