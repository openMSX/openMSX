// implementation based on:
// http://map.grauw.nl/resources/midi/ym2148.php

#include "YM2148.hh"
#include "MidiInDevice.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

// status register flags
static const unsigned STAT_TXRDY = 0x01; // Transmitter ready: no MIDI-out send is in progress
static const unsigned STAT_RXRDY = 0x02; // Receiver ready: a MIDI-in byte is available for the MSX
static const unsigned STAT_OE    = 0x10; // Overrun error (incoming data)
static const unsigned STAT_FE    = 0x20; // Framing error (incoming data)

// command register bits
static const unsigned CMD_TXEN  = 0x01; // Transmit enable
static const unsigned CMD_TXIE  = 0x02; // TxRDY interrupt enable
static const unsigned CMD_RXEN  = 0x04; // Receive enable
static const unsigned CMD_RXIE  = 0x08; // RxRDY interrupt enable
static const unsigned CMD_ER    = 0x10; // Error Reset
static const unsigned CMD_IR    = 0x80; // Internal Reset
// The meaning of bits 5 and 6 are unknown (they are used by the CX5M
// software). Some documentation *guesses* they are related to IM2
// IRQ handling.

static const EmuDuration BIT_DURATION = EmuDuration::hz(31250);
static const EmuDuration CHAR_DURATION = BIT_DURATION * 10; // 1 start-bit, 8 data-bits, 1 stop-bit

YM2148::YM2148(const std::string& name_, MSXMotherBoard& motherBoard)
	: MidiInConnector(motherBoard.getPluggingController(), name_ + "-MIDI-in")
	, syncRecv (motherBoard.getScheduler())
	, syncTrans(motherBoard.getScheduler())
	, rxIRQ(motherBoard, name_ + "-rx-IRQ")
	, txIRQ(motherBoard, name_ + "-tx-IRQ")
	, txBuffer1(0), txBuffer2(0) // avoid UMR
	, outConnector(motherBoard.getPluggingController(), name_ + "-MIDI-out")
{
	reset();
}

void YM2148::reset()
{
	syncRecv .removeSyncPoint();
	syncTrans.removeSyncPoint();
	rxIRQ.reset();
	txIRQ.reset();
	rxReady = false;
	rxBuffer = 0;
	status = 0;
	commandReg = 0;
}

// MidiInConnector sends a new character.
void YM2148::recvByte(byte value, EmuTime::param time)
{
	assert(acceptsData() && ready());

	if (status & STAT_RXRDY) {
		// So, there is a byte that has to be read by the MSX still!
		// This happens when the MSX program doesn't
		// respond fast enough to an earlier received byte.
		status |= STAT_OE;
		// TODO investigate: overwrite rxBuffer in case of overrun?
	} else {
		rxBuffer = value;
		status |= STAT_RXRDY;
		if (commandReg & CMD_RXIE) rxIRQ.set();
	}

	// Not ready now, but we will be in a while
	rxReady = false;
	syncRecv.setSyncPoint(time + CHAR_DURATION);
}

// Triggered when we're ready to receive the next character.
void YM2148::execRecv(EmuTime::param time)
{
	assert(commandReg & CMD_RXEN);
	assert(!rxReady);
	rxReady = true;
	getPluggedMidiInDev().signal(time); // trigger (possible) send of next char
}

// MidiInDevice querries whether it can send a new character 'now'.
bool YM2148::ready()
{
	return rxReady;
}

// MidiInDevice querries whether it can send characters at all.
bool YM2148::acceptsData()
{
	return (commandReg & CMD_RXEN) != 0;
}

// MidiInDevice informs us about the format of the data it will send
// (MIDI is always 1 start-bit, 8 data-bits, 1 stop-bit, no parity-bits).
void  YM2148::setDataBits(DataBits /*bits*/)
{
	// ignore
}
void YM2148::setStopBits(StopBits /*bits*/)
{
	// ignore
}
void YM2148::setParityBit(bool /*enable*/, ParityBit /*parity*/)
{
	// ignore
}

// MSX program reads the status register.
byte YM2148::readStatus(EmuTime::param /*time*/)
{
	return status;
}
byte YM2148::peekStatus(EmuTime::param /*time*/) const
{
	return status;
}

// MSX programs reads the data register.
byte YM2148::readData(EmuTime::param /*time*/)
{
	status &= ~STAT_RXRDY;
	rxIRQ.reset(); // no need to check CMD_RXIE
	return rxBuffer;
}
byte YM2148::peekData(EmuTime::param /*time*/) const
{
	return rxBuffer;
}

// MSX program writes the command register.
void YM2148::writeCommand(byte value)
{
	if (value & CMD_IR) {
		reset();
		return; // do not process any other commands
	}
	if (value & CMD_ER) {
		status &= ~(STAT_OE | STAT_FE);
		return;
	}

	byte diff = commandReg ^ value;
	commandReg = value;

	if (diff & CMD_RXEN) {
		if (commandReg & CMD_RXEN) {
			// disabled -> enabled
			rxReady = true;
		} else {
			// enabled -> disabled
			rxReady = false;
			syncRecv.removeSyncPoint();
			status &= ~STAT_RXRDY; // IRQ is handled below
		}
	}
	if (diff & CMD_TXEN) {
		if (commandReg & CMD_TXEN) {
			// disabled -> enabled
			status |= STAT_TXRDY; // IRQ is handled below
			// TODO transmitter is ready at this point, does this immediately trigger an IRQ (when IRQs are enabled)?
		} else {
			// enabled -> disabled
			status &= ~STAT_TXRDY; // IRQ handled below
			syncTrans.removeSyncPoint();
		}
	}

	// update IRQ status
	rxIRQ.set((value & CMD_RXIE) && (status & STAT_RXRDY));
	txIRQ.set((value & CMD_TXIE) && (status & STAT_TXRDY));
}

// MSX program writes the data register.
void YM2148::writeData(byte value, EmuTime::param time)
{
	if (!(commandReg & CMD_TXEN)) return;

	if (syncTrans.pendingSyncPoint()) {
		// We're still sending the previous character, only buffer
		// this one. Don't accept any further characters.
		txBuffer2 = value;
		status &= ~STAT_TXRDY;
		txIRQ.reset();
	} else {
		// Immediately start sending this character. We're still
		// ready to accept a next character.
		send(value, time);
	}
}

// Start sending a character. It takes a while before it's finished sending.
void YM2148::send(byte value, EmuTime::param time)
{
	txBuffer1 = value;
	syncTrans.setSyncPoint(time + CHAR_DURATION);
}

// Triggered when a character has finished sending.
void YM2148::execTrans(EmuTime::param time)
{
	assert(commandReg & CMD_TXEN);

	outConnector.recvByte(txBuffer1, time);

	if (status & STAT_TXRDY) {
		// No next character to send.
	} else {
		// There already is a next character, start sending that now
		// and accept a next one.
		status |= STAT_TXRDY;
		if (commandReg & CMD_TXIE) txIRQ.set();
		send(txBuffer2, time);
	}
}

// Any pending IRQs?
bool YM2148::pendingIRQ() const
{
	return rxIRQ.getState() || txIRQ.getState();
}

template<typename Archive>
void YM2148::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.template serializeBase<MidiInConnector>(*this);
		ar.serialize("outConnector", outConnector);

		ar.serialize("syncRecv",  syncRecv);
		ar.serialize("syncTrans", syncTrans);

		ar.serialize("rxIRQ", rxIRQ);
		ar.serialize("txIRQ", txIRQ);

		ar.serialize("rxReady",    rxReady);
		ar.serialize("rxBuffer",   rxBuffer);
		ar.serialize("txBuffer1",  txBuffer1);
		ar.serialize("txBuffer2",  txBuffer2);
		ar.serialize("status",     status);
		ar.serialize("commandReg", commandReg);
	}
}
INSTANTIATE_SERIALIZE_METHODS(YM2148);

} // namespace openmsx
