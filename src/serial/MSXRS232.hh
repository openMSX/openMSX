// $Id$

#ifndef __MSXRS232_HH__
#define __MSXRS232_HH__

#include "MSXDevice.hh"
#include "I8251.hh"
#include "IRQHelper.hh"
#include "RS232Connector.hh"
#include <memory>

namespace openmsx {

class I8254;
class Ram;
class Rom;
class ClockPin;

class MSXRS232 : public MSXDevice, public RS232Connector
{
public:
	MSXRS232(const XMLElement& config, const EmuTime& time);
	virtual ~MSXRS232();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte *getReadCacheLine(word start) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word start) const;

	// RS232Connector  (input)
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, const EmuTime& time);
	virtual bool ready();
	virtual bool acceptsData();

private:
	byte readStatus(const EmuTime& time);
	void setIRQMask(byte value);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	bool rxrdyIRQlatch;
	bool rxrdyIRQenabled;
	IRQHelper rxrdyIRQ;

	// counter 0 rx clock pin
	class Counter0 : public ClockPinListener {
	public:
		Counter0(MSXRS232& rs232);
		virtual ~Counter0();
		virtual void signal(ClockPin& pin,
					const EmuTime& time);
		virtual void signalPosEdge(ClockPin& pin,
					const EmuTime& time);
	private:
		MSXRS232& rs232;
	} cntr0;
	
	// counter 1 tx clock pin
	class Counter1 : public ClockPinListener {
	public:
		Counter1(MSXRS232& rs232);
		virtual ~Counter1();
		virtual void signal(ClockPin& pin,
					const EmuTime& time);
		virtual void signalPosEdge(ClockPin& pin,
					const EmuTime& time);
	private:
		MSXRS232& rs232;
	} cntr1;
	
	const std::auto_ptr<I8254> i8254;

	// I8251Interface
	class I8251Interf : public I8251Interface {
	public:
		I8251Interf(MSXRS232& rs232);
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
	} interf;
	
	const std::auto_ptr<I8251> i8251;
	const std::auto_ptr<Rom> rom;
	std::auto_ptr<Ram> ram;
};

} // namespace openmsx

#endif
