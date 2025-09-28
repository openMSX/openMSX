#include "MC6850.hh"

#include "MidiInDevice.hh"

#include "EmuTime.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include <array>

namespace openmsx {

// control register bits
static constexpr unsigned CR_CDS1 = 0x01; // Counter Divide Select 1
static constexpr unsigned CR_CDS2 = 0x02; // Counter Divide Select 2
static constexpr unsigned CR_CDS  = CR_CDS1 | CR_CDS2;
static constexpr unsigned CR_MR   = CR_CDS1 | CR_CDS2; // Master Reset
// CDS2 CDS1
// 0    0     divide by 1
// 0    1     divide by 16
// 1    0     divide by 64
// 1    1     master reset (!)

static constexpr unsigned CR_WS1  = 0x04; // Word Select 1 (mostly parity)
static constexpr unsigned CR_WS2  = 0x08; // Word Select 2 (mostly nof stop bits)
static constexpr unsigned CR_WS3  = 0x10; // Word Select 3: 7/8 bits
static constexpr unsigned CR_WS   = CR_WS1 | CR_WS2 | CR_WS3; // Word Select
// WS3 WS2 WS1
// 0   0   0   7 bits - 2 stop bits - Even parity
// 0   0   1   7 bits - 2 stop bits - Odd  parity
// 0   1   0   7 bits - 1 stop bit  - Even parity
// 0   1   1   7 bits - 1 stop bit  - Odd  Parity
// 1   0   0   8 bits - 2 stop bits - No   Parity
// 1   0   1   8 bits - 1 stop bit  - No   Parity
// 1   1   0   8 bits - 1 stop bit  - Even parity
// 1   1   1   8 bits - 1 stop bit  - Odd  parity

static constexpr unsigned CR_TC1  = 0x20; // Transmit Control 1
static constexpr unsigned CR_TC2  = 0x40; // Transmit Control 2
static constexpr unsigned CR_TC   = CR_TC1 | CR_TC2; // Transmit Control
// TC2 TC1
// 0   0   /RTS low,  Transmitting Interrupt disabled
// 0   1   /RTS low,  Transmitting Interrupt enabled
// 1   0   /RTS high, Transmitting Interrupt disabled
// 1   1   /RTS low,  Transmits a Break level on the Transmit Data Output.
//                                 Interrupt disabled

static constexpr unsigned CR_RIE  = 0x80; // Receive Interrupt Enable: interrupt
// at Receive Data Register Full, Overrun, low-to-high transition on the Data
// Carrier Detect (/DCD) signal line

// status register bits
static constexpr unsigned STAT_RDRF = 0x01; // Receive Data Register Full
static constexpr unsigned STAT_TDRE = 0x02; // Transmit Data Register Empty
static constexpr unsigned STAT_DCD  = 0x04; // Data Carrier Detect (/DCD)
static constexpr unsigned STAT_CTS  = 0x08; // Clear-to-Send (/CTS)
static constexpr unsigned STAT_FE   = 0x10; // Framing Error
static constexpr unsigned STAT_OVRN = 0x20; // Receiver Overrun
static constexpr unsigned STAT_PE   = 0x40; // Parity Error
static constexpr unsigned STAT_IRQ  = 0x80; // Interrupt Request (/IRQ)

MC6850::MC6850(const std::string& name_, MSXMotherBoard& motherBoard, unsigned clockFreq_)
	: MidiInConnector(motherBoard.getPluggingController(), name_ + "-in")
	, syncRecv (motherBoard.getScheduler())
	, syncTrans(motherBoard.getScheduler())
	, clockFreq(clockFreq_)
	, rxIRQ(motherBoard, name_ + "-rx-IRQ")
	, txIRQ(motherBoard, name_ + "-tx-IRQ")
	, outConnector(motherBoard.getPluggingController(), name_ + "-out")
{
	reset(EmuTime::zero());
	setDataFormat();
}

// (Re-)initialize chip to default values (Tx and Rx disabled)
void MC6850::reset(EmuTime time)
{
	syncRecv .removeSyncPoint();
	syncTrans.removeSyncPoint();
	txClock.reset(time);
	txClock.setFreq(clockFreq);
	rxIRQ.reset();
	txIRQ.reset();
	rxReady = false;
	txShiftRegValid = false;
	pendingOVRN = false;
	controlReg = CR_MR;
	statusReg = 0;
	rxDataReg = 0;
	setDataFormat();
}

uint8_t MC6850::readStatusReg() const
{
	return peekStatusReg();
}

uint8_t MC6850::peekStatusReg() const
{
	uint8_t result = statusReg;
	if (rxIRQ.getState() || txIRQ.getState()) result |= STAT_IRQ;
	return result;
}

uint8_t MC6850::readDataReg()
{
	uint8_t result = peekDataReg();
	statusReg &= uint8_t(~(STAT_RDRF | STAT_OVRN));
	if (pendingOVRN) {
		pendingOVRN = false;
		statusReg |= STAT_OVRN;
	}
	rxIRQ.reset();
	return result;
}

uint8_t MC6850::peekDataReg() const
{
	return rxDataReg;
}

void MC6850::writeControlReg(uint8_t value, EmuTime time)
{
	uint8_t diff = value ^ controlReg;
	if (diff & CR_CDS) {
		if ((value & CR_CDS) == CR_MR) {
			reset(time);
		} else {
			// we got out of MR state
			rxReady = true;
			statusReg |= STAT_TDRE;

			txClock.reset(time);
			switch (value & CR_CDS) {
			case 0: txClock.setFreq(clockFreq,  1); break;
			case 1: txClock.setFreq(clockFreq, 16); break;
			case 2: txClock.setFreq(clockFreq, 64); break;
			}
		}
	}

	controlReg = value;
	if (diff & CR_WS) setDataFormat();

	// update IRQ status
	rxIRQ.set(( value & CR_RIE) && (statusReg & STAT_RDRF));
	txIRQ.set(((value & CR_TC) == 0x20) && (statusReg & STAT_TDRE));
}

// Sync data-format related parameters with the current value of controlReg
void MC6850::setDataFormat()
{
	outConnector.setDataBits(controlReg & CR_WS3 ? DataBits::D8 : DataBits::D7);

	static constexpr std::array<StopBits, 8> stopBits = {
		StopBits::S2, StopBits::S2, StopBits::S1, StopBits::S1,
		StopBits::S2, StopBits::S1, StopBits::S1, StopBits::S1,
	};
	outConnector.setStopBits(stopBits[(controlReg & CR_WS) >> 2]);

	outConnector.setParityBit(
		(controlReg & (CR_WS3 | CR_WS2)) != 0x10, // enable
		(controlReg & CR_WS1) ? Parity::ODD : Parity::EVEN);

	// start-bits, data-bits, parity-bits, stop-bits
	static constexpr std::array<uint8_t, 8> len = {
		1 + 7 + 1 + 2,
		1 + 7 + 1 + 2,
		1 + 7 + 1 + 1,
		1 + 7 + 1 + 1,
		1 + 8 + 0 + 2,
		1 + 8 + 0 + 1,
		1 + 8 + 1 + 1,
		1 + 8 + 1 + 1,
	};
	charLen = len[(controlReg & CR_WS) >> 2];
}

void MC6850::writeDataReg(uint8_t value, EmuTime time)
{
	if ((controlReg & CR_CDS) == CR_MR) return;

	txDataReg = value;
	statusReg &= uint8_t(~STAT_TDRE);
	txIRQ.reset();

	if (syncTrans.pendingSyncPoint()) {
		// We're still sending the previous character, only
		// buffer this one. Don't accept any further characters
	} else {
		// We were not yet sending. Start sending at the next txClock.
		// Important: till that time TDRE should go low
		//  (MC6850 detection routine in Synthesix depends on this)
		txClock.advance(time); // clock edge before or at 'time'
		txClock += 1; // clock edge strictly after 'time'
		syncTrans.setSyncPoint(txClock.getTime());
	}
}

// Triggered between transmitted characters, including before the first and
// after the last character.
void MC6850::execTrans(EmuTime time)
{
	assert(txClock.getTime() == time);
	assert((controlReg & CR_CDS) != CR_MR);

	if (txShiftRegValid) {
		txShiftRegValid = false;
		outConnector.recvByte(txShiftReg, time);
	}

	if (statusReg & STAT_TDRE) {
		// No next character to send, we're done.
	} else {
		// There already is a next character, start sending that now
		// and accept a next one.
		statusReg |= STAT_TDRE;
		if ((controlReg & CR_TC) == 0x20) txIRQ.set();

		txShiftReg = txDataReg;
		txShiftRegValid = true;

		txClock += charLen;
		syncTrans.setSyncPoint(txClock.getTime());
	}
}

// MidiInConnector sends a new character.
void MC6850::recvByte(uint8_t value, EmuTime time)
{
	assert(acceptsData() && ready());

	if (statusReg & STAT_RDRF) {
		// So, there is a byte that has to be read by the MSX still!
		// This happens when the MSX program doesn't
		// respond fast enough to an earlier received byte.
		// The STAT_OVRN flag only becomes active once the prior valid
		// character has been read from the data register.
		pendingOVRN = true;
	} else {
		rxDataReg = value;
		statusReg |= STAT_RDRF;
	}
	// both for OVRN and RDRF an IRQ is raised
	if (controlReg & CR_RIE) rxIRQ.set();

	// Not ready now, but we will be in a while
	rxReady = false;

	// The MC6850 has separate TxCLK and RxCLK inputs, but both share a
	// common divider. This implementation hard-codes an input frequency
	// for both. Below we want the receive clock period, but it's OK
	// to calculate that as 'txClock.getPeriod()'.
	syncRecv.setSyncPoint(time + txClock.getPeriod() * charLen);
}

// Triggered when we're ready to receive the next character.
void MC6850::execRecv(EmuTime time)
{
	assert(acceptsData());
	assert(!rxReady);
	rxReady = true;
	getPluggedMidiInDev().signal(time); // trigger (possible) send of next char
}

// MidiInDevice queries whether it can send a new character 'now'.
bool MC6850::ready()
{
	return rxReady;
}

// MidiInDevice queries whether it can send characters at all.
bool MC6850::acceptsData()
{
	return (controlReg & CR_CDS) != CR_MR;
}

// MidiInDevice informs us about the format of the data it will send
// (MIDI is always 1 start-bit, 8 data-bits, 1 stop-bit, no parity-bits).
void  MC6850::setDataBits(DataBits /*bits*/)
{
	// ignore
}
void MC6850::setStopBits(StopBits /*bits*/)
{
	// ignore
}
void MC6850::setParityBit(bool /*enable*/, Parity /*parity*/)
{
	// ignore
}

// version 1: initial version
// version 2: added control
// version 3: actually working MC6850 with many more member variables
template<typename Archive>
void MC6850::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 3)) {
		ar.template serializeBase<MidiInConnector>(*this);
		ar.serialize("outConnector", outConnector,

		             "syncRecv",  syncRecv,
		             "syncTrans", syncTrans,

		             "txClock", txClock,
		             "rxIRQ", rxIRQ,
		             "txIRQ", txIRQ,

		             "rxReady",         rxReady,
		             "txShiftRegValid", txShiftRegValid,
		             "pendingOVRN",     pendingOVRN,

		             "rxDataReg",  rxDataReg,
		             "txDataReg",  txDataReg,
		             "txShiftReg", txShiftReg,
		             "controlReg", controlReg,
		             "statusReg",  statusReg);
	} else if (ar.versionAtLeast(version, 2)) {
		ar.serialize("control", controlReg);
	} else {
		controlReg = 3;
	}

	if constexpr (Archive::IS_LOADER) {
		setDataFormat();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MC6850);

} // namespace openmsx
