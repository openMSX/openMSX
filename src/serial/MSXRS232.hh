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
class BooleanSetting;

class MSXRS232 : public MSXDevice, public RS232Connector
{
public:
	explicit MSXRS232(const DeviceConfig& config);
	virtual ~MSXRS232();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	// TODO: implement peekMem, because the default isn't OK anymore
	virtual const byte *getReadCacheLine(word start) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word start) const;

	// RS232Connector  (input)
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, EmuTime::param time);
	virtual bool ready();
	virtual bool acceptsData();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readIOImpl(word port, EmuTime::param time);
	void writeIOImpl(word port, byte value, EmuTime::param time);

	byte readStatus(EmuTime::param time);
	void setIRQMask(byte value);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	const std::unique_ptr<Counter0> cntr0; // counter 0 rx clock pin
	const std::unique_ptr<Counter1> cntr1; // counter 1 tx clock pin
	const std::unique_ptr<I8254> i8254;
	const std::unique_ptr<I8251Interf> interf;
	const std::unique_ptr<I8251> i8251;
	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<Ram> ram;

	IRQHelper rxrdyIRQ;
	bool rxrdyIRQlatch;
	bool rxrdyIRQenabled;

	const bool hasMemoryBasedIo;
	bool ioAccessEnabled;

	const std::unique_ptr<BooleanSetting> switchSetting;

	friend class Counter0;
	friend class Counter1;
	friend class I8251Interf;
};
SERIALIZE_CLASS_VERSION(MSXRS232, 2);


} // namespace openmsx

#endif
