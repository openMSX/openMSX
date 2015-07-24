#include "MC6850.hh"
#include "MidiInDevice.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

// MSX interface:
// port R/W
// 0    W    Control Register
// 1    W    Transmit Data Register
// 4    R    Status Register
// 5    R    Receive Data Register

// control register bits
static const unsigned CR_CDS1 = 0x01; // Counter Divide Select 1
static const unsigned CR_CDS2 = 0x02; // Counter Divide Select 2
static const unsigned CR_CDS  = CR_CDS1 | CR_CDS2;
static const unsigned CR_MR   = CR_CDS1 | CR_CDS2; // Master Reset
// CDS2 CDS1
// 0    0     divide by 1
// 0    1     divide by 16
// 1    0     divide by 64
// 1    1     master reset (!)

static const unsigned CR_WS1  = 0x04; // Word Select 1 (mostly parity)
static const unsigned CR_WS2  = 0x08; // Word Select 2 (mostly nof stop bits)
static const unsigned CR_WS3  = 0x10; // Word Select 3: 7/8 bits
static const unsigned CR_WS   = CR_WS1 | CR_WS2 | CR_WS3; // Word Select
// WS3 WS2 WS1
// 0   0   0   7 bits - 2 stop bits - Even parity
// 0   0   1   7 bits - 2 stop bits - Odd  parity
// 0   1   0   7 bits - 1 stop bit  - Even parity
// 0   1   1   7 bits - 1 stop bit  - Odd  Parity
// 1   0   0   8 bits - 2 stop bits - No   Parity
// 1   0   1   8 bits - 1 stop bit  - No   Parity
// 1   1   0   8 bits - 1 stop bit  - Even parity
// 1   1   1   8 bits - 1 stop bit  - Odd  parity

static const unsigned CR_TC1  = 0x20; // Transmit Control 1
static const unsigned CR_TC2  = 0x40; // Transmit Control 2
static const unsigned CR_TC   = CR_TC1 | CR_TC2; // Transmit Control
// TC2 TC1
// 0   0   /RTS low,  Transmitting Interrupt disabled
// 0   1   /RTS low,  Transmitting Interrupt enabled
// 1   0   /RTS high, Transmitting Interrupt disabled
// 1   1   /RTS low,  Transmits a Break level on the Transmit Data Output.
//                                 Interrupt disabled

static const unsigned CR_RIE  = 0x80; // Receive Interrupt Enable: interrupt
// at Receive Data Register Full, Overrun, low-to-high transition on the Data
// Carrier Detect (/DCD) signal line

// status register bits
static const unsigned STAT_RDRF = 0x01; // Receive Data Register Full
static const unsigned STAT_TDRE = 0x02; // Transmit Data Register Empty
static const unsigned STAT_DCD  = 0x04; // Data Carrier Detect (/DCD)
static const unsigned STAT_CTS  = 0x08; // Clear-to-Send (/CTS)
static const unsigned STAT_FE   = 0x10; // Framing Error
static const unsigned STAT_OVRN = 0x20; // Receiver Overrun
static const unsigned STAT_PE   = 0x40; // Parity Error
static const unsigned STAT_IRQ  = 0x80; // Interrupt Request (/IRQ)


// Some existing Music-Module detection routines:
// - fac demo 5: does OUT 0,3 : OUT 0,21 : INP(4) and expects to read 2
// - tetris 2 special edition: does INP(4) and expects to read 0
// - Synthesix: does INP(4), expects 0; OUT 0,3 : OUT 0,21: INP(4) and expects
//   bit 1 to be 1 and bit 2, 3 and 7 to be 0. Then does OUT 5,0xFE : INP(4)
//   and expects bit 1 to be 0.
// I did some _very_basic_ investigation and found the following:
// - after a reset reading from port 4 returns 0
// - after initializing the control register, reading port 4 returns 0 (of
//   course this will change when you start to actually receive/transmit data)
// - writing any value with the lower 2 bits set to 1 returns to the initial
//   state, and reading port 4 again returns 0.
// -  ?INP(4) : OUT0,3 : ?INP(4) : OUT0,21 : ? INP(4) : OUT0,3 :  ?INP(4)
//    outputs: 0, 0, 2, 0

MC6850::MC6850(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(getMotherBoard().getPluggingController(), MSXDevice::getName() + "-in")
	, syncRecv (getMotherBoard().getScheduler())
	, syncTrans(getMotherBoard().getScheduler())
	, txClock(EmuTime::zero)
	, rxIRQ(getMotherBoard(), MSXDevice::getName() + "-rx-IRQ")
	, txIRQ(getMotherBoard(), MSXDevice::getName() + "-tx-IRQ")
	, txDataReg(0), txShiftReg(0) // avoid UMR
	, outConnector(getMotherBoard().getPluggingController(), MSXDevice::getName() + "-out")
{
	reset(EmuTime::zero);
	setDataFormat();
}

// (Re-)initialize chip to default values (Tx and Rx disabled)
void MC6850::reset(EmuTime::param time)
{
	syncRecv .removeSyncPoint();
	syncTrans.removeSyncPoint();
	txClock.reset(time);
	txClock.setFreq(500000); // 500kHz
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

byte MC6850::readIO(word port, EmuTime::param /*time*/)
{
	switch (port & 0x1) {
	case 0:
		return readStatusReg();
	case 1:
		return readDataReg();
	}
	UNREACHABLE;
	return 0xFF;
}

byte MC6850::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x1) {
	case 0:
		return peekStatusReg();
	case 1:
		return peekDataReg();
	}
	UNREACHABLE;
	return 0xFF;
}

void MC6850::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		writeControlReg(value, time);
		break;
	case 1:
		writeDataReg(value, time);
		break;
	}
}

byte MC6850::readStatusReg()
{
	return peekStatusReg();
}

byte MC6850::peekStatusReg() const
{
	byte result = statusReg;
	if (rxIRQ.getState() || txIRQ.getState()) result |= STAT_IRQ;
	return result;
}

byte MC6850::readDataReg()
{
	byte result = peekDataReg();
	statusReg &= ~(STAT_RDRF | STAT_OVRN);
	if (pendingOVRN) {
		pendingOVRN = false;
		statusReg |= STAT_OVRN;
	}
	rxIRQ.reset();
	return result;
}

byte MC6850::peekDataReg() const
{
	return rxDataReg;
}

void MC6850::writeControlReg(byte value, EmuTime::param time)
{
	byte diff = value ^ controlReg;
	if (diff & CR_CDS) {
		if ((value & CR_CDS) == CR_MR) {
			reset(time);
		} else {
			// we got out of MR state
			rxReady = true;
			statusReg |= STAT_TDRE;

			txClock.reset(time);
			switch (value & CR_CDS) {
			case 0: txClock.setFreq(500000,  1); break; // 500kHz
			case 1: txClock.setFreq(500000, 16); break; // 31250Hz (MIDI)
			case 2: txClock.setFreq(500000, 64); break; // 7812.5Hz
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
	outConnector.setDataBits(controlReg & CR_WS3 ? DATA_8 : DATA_7);

	StopBits stopBits[8] = {
		STOP_2, STOP_2, STOP_1, STOP_1,
		STOP_2, STOP_1, STOP_1, STOP_1,
	};
	outConnector.setStopBits(stopBits[(controlReg & CR_WS) >> 2]);

	outConnector.setParityBit(
		(controlReg & (CR_WS3 | CR_WS2)) != 0x10, // enable
		(controlReg & CR_WS1) ? ODD : EVEN);

	// start-bits, data-bits, parity-bits, stop-bits
	byte len[8] = {
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

void MC6850::writeDataReg(byte value, EmuTime::param time)
{
	if ((controlReg & CR_CDS) == CR_MR) return;

	txDataReg = value;
	statusReg &= ~STAT_TDRE;
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
void MC6850::execTrans(EmuTime::param time)
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
		if (((controlReg & CR_TC) == 0x20)) txIRQ.set();

		txShiftReg = txDataReg;
		txShiftRegValid = true;

		txClock += charLen;
		syncTrans.setSyncPoint(txClock.getTime());
	}
}

// MidiInConnector sends a new character.
void MC6850::recvByte(byte value, EmuTime::param time)
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
	// common divider. This implementation hard-codes an input frequency of
	// 500kHz for both. Below we want the receive clock period, but it's OK
	// to calculate that as 'txClock.getPeriod()'.
	syncRecv.setSyncPoint(time + txClock.getPeriod() * charLen);
}

// Triggered when we're ready to receive the next character.
void MC6850::execRecv(EmuTime::param time)
{
	assert(acceptsData());
	assert(!rxReady);
	rxReady = true;
	getPluggedMidiInDev().signal(time); // trigger (possible) send of next char
}

// MidiInDevice querries whether it can send a new character 'now'.
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
void MC6850::setParityBit(bool /*enable*/, ParityBit /*parity*/)
{
	// ignore
}

// version 1: initial version
// version 2: added control
// version 3: actually working MC6850 with many more member variables
template<typename Archive>
void MC6850::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 3)) {
		ar.template serializeBase<MidiInConnector>(*this);
		ar.serialize("outConnector", outConnector);

		ar.serialize("syncRecv",  syncRecv);
		ar.serialize("syncTrans", syncTrans);

		ar.serialize("txClock", txClock);
		ar.serialize("rxIRQ", rxIRQ);
		ar.serialize("txIRQ", txIRQ);

		ar.serialize("rxReady",         rxReady);
		ar.serialize("txShiftRegValid", txShiftRegValid);
		ar.serialize("pendingOVRN",     pendingOVRN);

		ar.serialize("rxDataReg",  rxDataReg);
		ar.serialize("txDataReg",  txDataReg);
		ar.serialize("txShiftReg", txShiftReg);
		ar.serialize("controlReg", controlReg);
		ar.serialize("statusReg",  statusReg);
	} else if (ar.versionAtLeast(version, 2)) {
		ar.serialize("control", controlReg);
	} else {
		controlReg = 3;
	}

	if (ar.isLoader()) {
		setDataFormat();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MC6850);
REGISTER_MSXDEVICE(MC6850, "MC6850");

} // namespace openmsx
