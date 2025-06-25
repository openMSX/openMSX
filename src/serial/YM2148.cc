// implementation based on:
// http://map.grauw.nl/resources/midi/ym2148.php

#include "YM2148.hh"
#include "MidiInDevice.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

// status register flags
static constexpr unsigned STAT_TXRDY = 0x01; // Transmitter ready: no MIDI-out send is in progress
static constexpr unsigned STAT_RXRDY = 0x02; // Receiver ready: a MIDI-in byte is available for the MSX
static constexpr unsigned STAT_OE    = 0x10; // Overrun error (incoming data)
static constexpr unsigned STAT_FE    = 0x20; // Framing error (incoming data)

// command register bits
static constexpr unsigned CMD_TXEN  = 0x01; // Transmit enable
static constexpr unsigned CMD_TXIE  = 0x02; // TxRDY interrupt enable
static constexpr unsigned CMD_RXEN  = 0x04; // Receive enable
static constexpr unsigned CMD_RXIE  = 0x08; // RxRDY interrupt enable
static constexpr unsigned CMD_ER    = 0x10; // Error Reset
static constexpr unsigned CMD_IR    = 0x80; // Internal Reset
// The meaning of bits 5 and 6 are unknown (they are used by the CX5M
// software). Some documentation *guesses* they are related to IM2
// IRQ handling.

static constexpr auto BIT_DURATION = EmuDuration::hz(31250);
static constexpr auto CHAR_DURATION = BIT_DURATION * 10; // 1 start-bit, 8 data-bits, 1 stop-bit

YM2148::YM2148(const std::string& name_, MSXMotherBoard& motherBoard)
	: MidiInConnector(motherBoard.getPluggingController(), name_ + "-MIDI-in")
	, syncRecv (motherBoard.getScheduler())
	, syncTrans(motherBoard.getScheduler())
	, rxIRQ(motherBoard, name_ + "-rx-IRQ")
	, txIRQ(motherBoard, name_ + "-tx-IRQ")
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
void YM2148::recvByte(uint8_t value, EmuTime time)
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
void YM2148::execRecv(EmuTime time)
{
	assert(commandReg & CMD_RXEN);
	assert(!rxReady);
	rxReady = true;
	getPluggedMidiInDev().signal(time); // trigger (possible) send of next char
}

// MidiInDevice queries whether it can send a new character 'now'.
bool YM2148::ready()
{
	return rxReady;
}

// MidiInDevice queries whether it can send characters at all.
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
void YM2148::setParityBit(bool /*enable*/, Parity /*parity*/)
{
	// ignore
}

// MSX program reads the status register.
uint8_t YM2148::readStatus(EmuTime /*time*/) const
{
	return status;
}
uint8_t YM2148::peekStatus(EmuTime /*time*/) const
{
	return status;
}

// MSX programs reads the data register.
uint8_t YM2148::readData(EmuTime /*time*/)
{
	status &= uint8_t(~STAT_RXRDY);
	rxIRQ.reset(); // no need to check CMD_RXIE
	return rxBuffer;
}
uint8_t YM2148::peekData(EmuTime /*time*/) const
{
	return rxBuffer;
}

// MSX program writes the command register.
void YM2148::writeCommand(uint8_t value)
{
	if (value & CMD_IR) {
		reset();
		return; // do not process any other commands
	}
	if (value & CMD_ER) {
		status &= uint8_t(~(STAT_OE | STAT_FE));
		return;
	}

	uint8_t diff = commandReg ^ value;
	commandReg = value;

	if (diff & CMD_RXEN) {
		if (commandReg & CMD_RXEN) {
			// disabled -> enabled
			rxReady = true;
		} else {
			// enabled -> disabled
			rxReady = false;
			syncRecv.removeSyncPoint();
			status &= uint8_t(~STAT_RXRDY); // IRQ is handled below
		}
	}
	if (diff & CMD_TXEN) {
		if (commandReg & CMD_TXEN) {
			// disabled -> enabled
			status |= STAT_TXRDY; // IRQ is handled below
			// TODO transmitter is ready at this point, does this immediately trigger an IRQ (when IRQs are enabled)?
		} else {
			// enabled -> disabled
			status &= uint8_t(~STAT_TXRDY); // IRQ handled below
			syncTrans.removeSyncPoint();
		}
	}

	// update IRQ status
	rxIRQ.set((value & CMD_RXIE) && (status & STAT_RXRDY));
	txIRQ.set((value & CMD_TXIE) && (status & STAT_TXRDY));
}

// MSX program writes the data register.
void YM2148::writeData(uint8_t value, EmuTime time)
{
	if (!(commandReg & CMD_TXEN)) return;

	if (syncTrans.pendingSyncPoint()) {
		// We're still sending the previous character, only buffer
		// this one. Don't accept any further characters.
		txBuffer2 = value;
		status &= uint8_t(~STAT_TXRDY);
		txIRQ.reset();
	} else {
		// Immediately start sending this character. We're still
		// ready to accept a next character.
		send(value, time);
	}
}

// Start sending a character. It takes a while before it's finished sending.
void YM2148::send(uint8_t value, EmuTime time)
{
	txBuffer1 = value;
	syncTrans.setSyncPoint(time + CHAR_DURATION);
}

// Triggered when a character has finished sending.
void YM2148::execTrans(EmuTime time)
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
		ar.serialize("outConnector", outConnector,

		             "syncRecv",     syncRecv,
		             "syncTrans",    syncTrans,

		             "rxIRQ",        rxIRQ,
		             "txIRQ",        txIRQ,

		             "rxReady",      rxReady,
		             "rxBuffer",     rxBuffer,
		             "txBuffer1",    txBuffer1,
		             "txBuffer2",    txBuffer2,
		             "status",       status,
		             "commandReg",   commandReg);
	}
}
INSTANTIATE_SERIALIZE_METHODS(YM2148);

} // namespace openmsx
