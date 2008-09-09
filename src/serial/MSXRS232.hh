// $Id$

#ifndef MSXRS232_HH
#define MSXRS232_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "RS232Connector.hh"
#include <memory>

namespace openmsx {

class Counter0;
class Counter1;
class I8254;
class I8251;
class I8251Interf;
class Ram;
class Rom;
class ClockPin;

class MSXRS232 : public MSXDevice, public RS232Connector
{
public:
	MSXRS232(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXRS232();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readStatus(const EmuTime& time);
	void setIRQMask(byte value);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	const std::auto_ptr<Counter0> cntr0; // counter 0 rx clock pin
	const std::auto_ptr<Counter1> cntr1; // counter 1 tx clock pin
	const std::auto_ptr<I8254> i8254;
	const std::auto_ptr<I8251Interf> interf;
	const std::auto_ptr<I8251> i8251;
	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<Ram> ram;

	IRQHelper rxrdyIRQ;
	bool rxrdyIRQlatch;
	bool rxrdyIRQenabled;

	friend class Counter0;
	friend class Counter1;
	friend class I8251Interf;
};

REGISTER_MSXDEVICE(MSXRS232, "RS232");

} // namespace openmsx

#endif
