// $Id$

#include "MSXRS232.hh"
#include "RS232Device.hh"
#include "MSXMotherBoard.hh"
#include "I8254.hh"
#include "I8251.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

const unsigned RAM_OFFSET = 0x2000;
const unsigned RAM_SIZE = 0x800;


class Counter0 : public ClockPinListener
{
public:
	explicit Counter0(MSXRS232& rs232);
	virtual ~Counter0();
	virtual void signal(ClockPin& pin, const EmuTime& time);
	virtual void signalPosEdge(ClockPin& pin, const EmuTime& time);
private:
	MSXRS232& rs232;
};

class Counter1 : public ClockPinListener
{
public:
	explicit Counter1(MSXRS232& rs232);
	virtual ~Counter1();
	virtual void signal(ClockPin& pin, const EmuTime& time);
	virtual void signalPosEdge(ClockPin& pin, const EmuTime& time);
private:
	MSXRS232& rs232;
};

class I8251Interf : public I8251Interface
{
public:
	explicit I8251Interf(MSXRS232& rs232);
	virtual ~I8251Interf();
	virtual void setRxRDY(bool status, const EmuTime& time);
	virtual void setDTR(bool status, const EmuTime& time);
	virtual void setRTS(bool status, const EmuTime& time);
	virtual bool getDSR(const EmuTime& time);
	virtual bool getCTS(const EmuTime& time);
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, const EmuTime& time);
	virtual void signal(const EmuTime& time);
private:
	MSXRS232& rs232;
};


MSXRS232::MSXRS232(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, RS232Connector(motherBoard.getPluggingController(), "msx-rs232")
	, cntr0(new Counter0(*this))
	, cntr1(new Counter1(*this))
	, i8254(new I8254(motherBoard.getScheduler(),
	                  cntr0.get(), cntr1.get(), NULL, getCurrentTime()))
	, interf(new I8251Interf(*this))
	, i8251(new I8251(motherBoard.getScheduler(), *interf,
	                  getCurrentTime()))
	, rom(new Rom(motherBoard, MSXDevice::getName() + " ROM", "rom", config))
	, ram(config.getChildDataAsBool("ram", false)
	      ? new Ram(motherBoard, MSXDevice::getName() + " RAM",
	                "RS232 RAM", RAM_SIZE)
	      : NULL)
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, rxrdyIRQlatch(false)
	, rxrdyIRQenabled(false)
{
	EmuDuration total(1.0 / 1.8432e6); // 1.8432MHz
	EmuDuration hi   (1.0 / 3.6864e6); //   half clock period
	const EmuTime& time = getCurrentTime();
	i8254->getClockPin(0).setPeriodicState(total, hi, time);
	i8254->getClockPin(1).setPeriodicState(total, hi, time);
	i8254->getClockPin(2).setPeriodicState(total, hi, time);

	reset(time);
}

MSXRS232::~MSXRS232()
{
}

void MSXRS232::reset(const EmuTime& /*time*/)
{
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
	rxrdyIRQ.reset();

	if (ram.get()) {
		ram->clear();
	}
}

byte MSXRS232::readMem(word address, const EmuTime& /*time*/)
{
	word addr = address & 0x3FFF;
	if (ram.get() && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return (*ram)[addr - RAM_OFFSET];
	} else if ((0x4000 <= address) && (address < 0x8000)) {
		return (*rom)[addr];
	} else {
		return 0xFF;
	}
}

const byte* MSXRS232::getReadCacheLine(word start) const
{
	word addr = start & 0x3FFF;
	if (ram.get() && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return &(*ram)[addr - RAM_OFFSET];
	} else if ((0x4000 <= start) && (start < 0x8000)) {
		return &(*rom)[addr];
	} else {
		return unmappedRead;
	}
}

void MSXRS232::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	word addr = address & 0x3FFF;
	if (ram.get() && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		(*ram)[addr - RAM_OFFSET] = value;
	}
}

byte* MSXRS232::getWriteCacheLine(word start) const
{
	return NULL;
	word addr = start & 0x3FFF;
	if (ram.get() && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return &(*ram)[addr - RAM_OFFSET];
	} else {
		return unmappedWrite;
	}
}

byte MSXRS232::readIO(word port, const EmuTime& time)
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251->readIO(port, time);
			break;
		case 2: // Status sense port
			result = readStatus(time);
			break;
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254->readIO(port - 4, time);
			break;
		default:
			assert(false);
			result = 0xFF;	// avoid warning
	}
	//PRT_DEBUG("MSXRS232 read " << (int)port << " " << (int)result);
	return result;
}

byte MSXRS232::peekIO(word port, const EmuTime& time) const
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251->peekIO(port, time);
			break;
		case 2: // Status sense port
			result = 0; // TODO not implemented
			break;
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254->peekIO(port - 4, time);
			break;
		default:
			assert(false);
			result = 0xFF;	// avoid warning
	}
	return result;
}

void MSXRS232::writeIO(word port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("MSXRS232 write " << (int)port << " " << (int)value);
	port &= 0x07;
	switch (port & 0x07) {
		case 0: // UART data register
		case 1: // UART command register
			i8251->writeIO(port, value, time);
			break;
		case 2: // interrupt mask register
			setIRQMask(value);
			break;
		case 3: // no function
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			i8254->writeIO(port - 4, value, time);
			break;
	}
}

byte MSXRS232::readStatus(const EmuTime& time)
{
	byte result = 0; // TODO check unused bits
	if (!interf->getCTS(time)) {
		result |= 0x80;
	}
	if (i8254->getOutputPin(2).getState(time)) {
		result |= 0x40;
	}
	return result;
}

void MSXRS232::setIRQMask(byte value)
{
	enableRxRDYIRQ(!(value & 1));
}

void MSXRS232::setRxRDYIRQ(bool status)
{
	if (rxrdyIRQlatch != status) {
		rxrdyIRQlatch = status;
		if (rxrdyIRQenabled) {
			if (rxrdyIRQlatch) {
				rxrdyIRQ.set();
			} else {
				rxrdyIRQ.reset();
			}
		}
	}
}

void MSXRS232::enableRxRDYIRQ(bool enabled)
{
	if (rxrdyIRQenabled != enabled) {
		rxrdyIRQenabled = enabled;
		if (!rxrdyIRQenabled && rxrdyIRQlatch) {
			rxrdyIRQ.reset();
		}
	}
}


// I8251Interface  (pass calls from I8251 to outConnector)

I8251Interf::I8251Interf(MSXRS232& rs232_)
	: rs232(rs232_)
{
}

I8251Interf::~I8251Interf()
{
}

void I8251Interf::setRxRDY(bool status, const EmuTime& /*time*/)
{
	rs232.setRxRDYIRQ(status);
}

void I8251Interf::setDTR(bool status, const EmuTime& time)
{
	rs232.getPluggedRS232Dev().setDTR(status, time);
}

void I8251Interf::setRTS(bool status, const EmuTime& time)
{
	rs232.getPluggedRS232Dev().setRTS(status, time);
}

bool I8251Interf::getDSR(const EmuTime& time)
{
	return rs232.getPluggedRS232Dev().getDSR(time);
}

bool I8251Interf::getCTS(const EmuTime& time)
{
	return rs232.getPluggedRS232Dev().getCTS(time);
}

void I8251Interf::setDataBits(DataBits bits)
{
	rs232.getPluggedRS232Dev().setDataBits(bits);
}

void I8251Interf::setStopBits(StopBits bits)
{
	rs232.getPluggedRS232Dev().setStopBits(bits);
}

void I8251Interf::setParityBit(bool enable, ParityBit parity)
{
	rs232.getPluggedRS232Dev().setParityBit(enable, parity);
}

void I8251Interf::recvByte(byte value, const EmuTime& time)
{
	rs232.getPluggedRS232Dev().recvByte(value, time);
}

void I8251Interf::signal(const EmuTime& time)
{
	rs232.getPluggedRS232Dev().signal(time); // for input
}


// Counter 0 output

Counter0::Counter0(MSXRS232& rs232_)
	: rs232(rs232_)
{
}

Counter0::~Counter0()
{
}

void Counter0::signal(ClockPin& pin, const EmuTime& time)
{
	ClockPin& clk = rs232.i8251->getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void Counter0::signalPosEdge(ClockPin& /*pin*/, const EmuTime& /*time*/)
{
	assert(false);
}


// Counter 1 output // TODO split rx tx

Counter1::Counter1(MSXRS232& rs232_)
	: rs232(rs232_)
{
}

Counter1::~Counter1()
{
}

void Counter1::signal(ClockPin& pin, const EmuTime& time)
{
	ClockPin& clk = rs232.i8251->getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void Counter1::signalPosEdge(ClockPin& /*pin*/, const EmuTime& /*time*/)
{
	assert(false);
}


// RS232Connector input

bool MSXRS232::ready()
{
	return i8251->isRecvReady();
}

bool MSXRS232::acceptsData()
{
	return i8251->isRecvEnabled();
}

void MSXRS232::setDataBits(DataBits bits)
{
	i8251->setDataBits(bits);
}

void MSXRS232::setStopBits(StopBits bits)
{
	i8251->setStopBits(bits);
}

void MSXRS232::setParityBit(bool enable, ParityBit parity)
{
	i8251->setParityBit(enable, parity);
}

void MSXRS232::recvByte(byte value, const EmuTime& time)
{
	i8251->recvByte(value, time);
}


template<typename Archive>
void MSXRS232::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<RS232Connector>(*this);

	ar.serialize("I8254", *i8254);
	ar.serialize("I8251", *i8251);
	if (ram.get()) {
		ar.serialize("ram", *ram);
	}
	ar.serialize("rxrdyIRQ", rxrdyIRQ);
	ar.serialize("rxrdyIRQlatch", rxrdyIRQlatch);
	ar.serialize("rxrdyIRQenabled", rxrdyIRQenabled);

	// don't serialize cntr0, cntr1, interf
}
INSTANTIATE_SERIALIZE_METHODS(MSXRS232);
REGISTER_MSXDEVICE(MSXRS232, "RS232");

} // namespace openmsx

