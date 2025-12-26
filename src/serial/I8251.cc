#include "I8251.hh"

#include "serialize.hh"

#include "unreachable.hh"

#include <cassert>

namespace openmsx {

static constexpr uint8_t STAT_TXRDY   = 0x01;
static constexpr uint8_t STAT_RXRDY   = 0x02;
static constexpr uint8_t STAT_TXEMPTY = 0x04;
static constexpr uint8_t STAT_PE      = 0x08;
static constexpr uint8_t STAT_OE      = 0x10;
static constexpr uint8_t STAT_FE      = 0x20;
static constexpr uint8_t STAT_SYN_BRK = 0x40;
static constexpr uint8_t STAT_DSR     = 0x80;

static constexpr uint8_t MODE_BAUDRATE    = 0x03;
static constexpr uint8_t MODE_SYNCHRONOUS = 0x00;
static constexpr uint8_t MODE_RATE1       = 0x01;
static constexpr uint8_t MODE_RATE16      = 0x02;
static constexpr uint8_t MODE_RATE64      = 0x03;
static constexpr uint8_t MODE_WORD_LENGTH  = 0x0C;
static constexpr uint8_t MODE_5BIT        = 0x00;
static constexpr uint8_t MODE_6BIT        = 0x04;
static constexpr uint8_t MODE_7BIT        = 0x08;
static constexpr uint8_t MODE_8BIT        = 0x0C;
static constexpr uint8_t MODE_PARITY_EVEN = 0x10;
static constexpr uint8_t MODE_PARITY_ODD  = 0x00;
static constexpr uint8_t MODE_PARITEVEN   = 0x20;
static constexpr uint8_t MODE_STOP_BITS   = 0xC0;
static constexpr uint8_t MODE_STOP_INV    = 0x00;
static constexpr uint8_t MODE_STOP_1      = 0x40;
static constexpr uint8_t MODE_STOP_15     = 0x80;
static constexpr uint8_t MODE_STOP_2      = 0xC0;
static constexpr uint8_t MODE_SINGLE_SYNC = 0x80;

static constexpr uint8_t CMD_TXEN    = 0x01;
static constexpr uint8_t CMD_DTR     = 0x02;
static constexpr uint8_t CMD_RXE     = 0x04;
static constexpr uint8_t CMD_SBRK    = 0x08;
static constexpr uint8_t CMD_RST_ERR = 0x10;
static constexpr uint8_t CMD_RTS     = 0x20;
static constexpr uint8_t CMD_RESET   = 0x40;
static constexpr uint8_t CMD_HUNT    = 0x80;


I8251::I8251(Scheduler& scheduler, I8251Interface& interface_, EmuTime time)
	: syncRecv (scheduler)
	, syncTrans(scheduler)
	, interface(interface_), clock(scheduler)
{
	reset(time);
}

void I8251::reset(EmuTime time)
{
	// initialize these to avoid UMR on savestate
	//   TODO investigate correct initial state after reset
	charLength = 0;
	recvDataBits  = SerialDataInterface::DataBits::D8;
	recvStopBits  = SerialDataInterface::StopBits::S1;
	recvParityBit = SerialDataInterface::Parity::EVEN;
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
	cmdPhase = CmdPhase::MODE;
}

uint8_t I8251::readIO(uint16_t port, EmuTime time)
{
	switch (port & 1) {
		case 0:  return readTrans(time);
		case 1:  return readStatus(time);
		default: UNREACHABLE;
	}
}

uint8_t I8251::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 1) {
		case 0:  return recvBuf;
		case 1:  return status; // TODO peekStatus()
		default: UNREACHABLE;
	}
}


void I8251::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	switch (port & 1) {
	case 0:
		writeTrans(value, time);
		break;
	case 1:
		switch (cmdPhase) {
		using enum CmdPhase;
		case MODE:
			setMode(value);
			if ((mode & MODE_BAUDRATE) == MODE_SYNCHRONOUS) {
				cmdPhase = SYNC1;
			} else {
				cmdPhase = CMD;
			}
			break;
		case SYNC1:
			sync1 = value;
			if (mode & MODE_SINGLE_SYNC) {
				cmdPhase = CMD;
			} else {
				cmdPhase = SYNC2;
			}
			break;
		case SYNC2:
			sync2 = value;
			cmdPhase = CMD;
			break;
		case CMD:
			if (value & CMD_RESET) {
				cmdPhase = MODE;
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

void I8251::setMode(uint8_t newMode)
{
	mode = newMode;

	auto dataBits = [&] {
		switch (mode & MODE_WORD_LENGTH) {
		using enum SerialDataInterface::DataBits;
		case MODE_5BIT: return D5;
		case MODE_6BIT: return D6;
		case MODE_7BIT: return D7;
		case MODE_8BIT: return D8;
		default: UNREACHABLE;
		}
	}();
	interface.setDataBits(dataBits);

	auto stopBits = [&] {
		switch(mode & MODE_STOP_BITS) {
		using enum SerialDataInterface::StopBits;
		case MODE_STOP_INV: return INV;
		case MODE_STOP_1:   return S1;
		case MODE_STOP_15:  return S1_5;
		case MODE_STOP_2:   return S2;
		default: UNREACHABLE;
		}
	}();
	interface.setStopBits(stopBits);

	bool parityEnable = (mode & MODE_PARITY_EVEN) != 0;
	SerialDataInterface::Parity parity = (mode & MODE_PARITEVEN) ?
		SerialDataInterface::Parity::EVEN : SerialDataInterface::Parity::ODD;
	interface.setParityBit(parityEnable, parity);

	unsigned baudrate = [&] {
		switch (mode & MODE_BAUDRATE) {
		case MODE_SYNCHRONOUS: return 1;
		case MODE_RATE1:       return 1;
		case MODE_RATE16:      return 16;
		case MODE_RATE64:      return 64;
		default: UNREACHABLE;
		}
	}();

	charLength = (((2 * (1 + unsigned(dataBits) + (parityEnable ? 1 : 0))) +
	               unsigned(stopBits)) * baudrate) / 2;
}

void I8251::writeCommand(uint8_t value, EmuTime time)
{
	uint8_t oldCommand = command;
	command = value;

	// CMD_RESET, CMD_TXEN, CMD_RXE  handled in other routines

	interface.setRTS((command & CMD_RTS) != 0, time);
	interface.setDTR((command & CMD_DTR) != 0, time);

	if (!(command & CMD_TXEN)) {
		// disable transmitter
		syncTrans.removeSyncPoint();
		status |= STAT_TXRDY | STAT_TXEMPTY;
	}
	if (command & CMD_RST_ERR) {
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
		interface.signal(time);
	}
}

uint8_t I8251::readStatus(EmuTime time)
{
	uint8_t result = status;
	if (interface.getDSR(time)) {
		result |= STAT_DSR;
	}
	return result;
}

uint8_t I8251::readTrans(EmuTime time)
{
	status &= ~STAT_RXRDY;
	interface.setRxRDY(false, time);
	return recvBuf;
}

void I8251::writeTrans(uint8_t value, EmuTime time)
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

void I8251::setParityBit(bool enable, Parity parity)
{
	recvParityEnabled = enable;
	recvParityBit = parity;
}

void I8251::recvByte(uint8_t value, EmuTime time)
{
	// TODO STAT_PE / STAT_FE / STAT_SYN_BRK
	assert(recvReady && (command & CMD_RXE));
	if (status & STAT_RXRDY) {
		status |= STAT_OE;
	} else {
		recvBuf = value;
		status |= STAT_RXRDY;
		interface.setRxRDY(true, time);
	}
	recvReady = false;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		syncRecv.setSyncPoint(next);
	}
}

bool I8251::isRecvEnabled() const
{
	return (command & CMD_RXE) != 0;
}

void I8251::send(uint8_t value, EmuTime time)
{
	status &= ~STAT_TXEMPTY;
	sendByte = value;
	if (clock.isPeriodic()) {
		EmuTime next = time + (clock.getTotalDuration() * charLength);
		syncTrans.setSyncPoint(next);
	}
}

void I8251::execRecv(EmuTime time)
{
	assert(command & CMD_RXE);
	recvReady = true;
	interface.signal(time);
}

void I8251::execTrans(EmuTime time)
{
	assert(!(status & STAT_TXEMPTY) && (command & CMD_TXEN));

	interface.recvByte(sendByte, time);
	if (status & STAT_TXRDY) {
		status |= STAT_TXEMPTY;
	} else {
		status |= STAT_TXRDY;
		send(sendBuffer, time);
	}
}


static constexpr auto dataBitsInfo = std::to_array<enum_string<SerialDataInterface::DataBits>>({
	{ "5", SerialDataInterface::DataBits::D5 },
	{ "6", SerialDataInterface::DataBits::D6 },
	{ "7", SerialDataInterface::DataBits::D7 },
	{ "8", SerialDataInterface::DataBits::D8 },
});
SERIALIZE_ENUM(SerialDataInterface::DataBits, dataBitsInfo);

static constexpr auto stopBitsInfo = std::to_array<enum_string<SerialDataInterface::StopBits>>({
	{ "INVALID", SerialDataInterface::StopBits::INV },
	{ "1",       SerialDataInterface::StopBits::S1   },
	{ "1.5",     SerialDataInterface::StopBits::S1_5  },
	{ "2",       SerialDataInterface::StopBits::S2   },
});
SERIALIZE_ENUM(SerialDataInterface::StopBits, stopBitsInfo);

static constexpr auto parityBitInfo = std::to_array<enum_string<SerialDataInterface::Parity>>({
	{ "EVEN", SerialDataInterface::Parity::EVEN },
	{ "ODD",  SerialDataInterface::Parity::ODD  },
});
SERIALIZE_ENUM(SerialDataInterface::Parity, parityBitInfo);

static constexpr auto cmdFazeInfo = std::to_array<enum_string<I8251::CmdPhase>>({
	{ "MODE",  I8251::CmdPhase::MODE  },
	{ "SYNC1", I8251::CmdPhase::SYNC1 },
	{ "SYNC2", I8251::CmdPhase::SYNC2 },
	{ "CMD",   I8251::CmdPhase::CMD   },
});
SERIALIZE_ENUM(I8251::CmdPhase, cmdFazeInfo);

// version 1: initial version
// version 2: removed 'userData' from Schedulable
template<typename Archive>
void I8251::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("syncRecv",  syncRecv,
		             "syncTrans", syncTrans);
	} else {
		Schedulable::restoreOld(ar, {&syncRecv, &syncTrans});
	}
	ar.serialize("clock",             clock,
	             "charLength",        charLength,
	             "recvDataBits",      recvDataBits,
	             "recvStopBits",      recvStopBits,
	             "recvParityBit",     recvParityBit,
	             "recvParityEnabled", recvParityEnabled,
	             "recvBuf",           recvBuf,
	             "recvReady",         recvReady,
	             "sendByte",          sendByte,
	             "sendBuffer",        sendBuffer,
	             "status",            status,
	             "command",           command,
	             "mode",              mode,
	             "sync1",             sync1,
	             "sync2",             sync2,
	             "cmdFaze",           cmdPhase); // TODO fix spelling error if we ever need to upgrade this savestate format
}
INSTANTIATE_SERIALIZE_METHODS(I8251);

} // namespace openmsx
