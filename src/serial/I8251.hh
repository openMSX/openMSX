// $Id$

// This class implements the Intel 8251 chip (UART)

#ifndef I8251_HH
#define I8251_HH

#include "ClockPin.hh"
#include "SerialDataInterface.hh"
#include "Schedulable.hh"
#include "openmsx.hh"

namespace openmsx {

class I8251Interface : public SerialDataInterface
{
public:
	virtual ~I8251Interface() {}
	virtual void setRxRDY(bool status, EmuTime::param time) = 0;
	virtual void setDTR(bool status, EmuTime::param time) = 0;
	virtual void setRTS(bool status, EmuTime::param time) = 0;
	virtual bool getDSR(EmuTime::param time) = 0;
	virtual bool getCTS(EmuTime::param time) = 0; // TODO use this
	virtual void signal(EmuTime::param time) = 0;
};

class I8251 : public SerialDataInterface, public Schedulable
{
public:
	I8251(Scheduler& scheduler, I8251Interface& interf, EmuTime::param time);

	void reset(EmuTime::param time);
	byte readIO(word port, EmuTime::param time);
	byte peekIO(word port, EmuTime::param time) const;
	void writeIO(word port, byte value, EmuTime::param time);
	ClockPin& getClockPin();
	bool isRecvReady();
	bool isRecvEnabled();

	// SerialDataInterface
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, EmuTime::param time);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum CmdFaze {
		FAZE_MODE, FAZE_SYNC1, FAZE_SYNC2, FAZE_CMD
	};

private:
	void setMode(byte mode);
	void writeCommand(byte value, EmuTime::param time);
	byte readStatus(EmuTime::param time);
	byte readTrans(EmuTime::param time);
	void writeTrans(byte value, EmuTime::param time);
	void send(byte value, EmuTime::param time);

	I8251Interface& interf;
	ClockPin clock;
	unsigned charLength;

	CmdFaze cmdFaze;
	SerialDataInterface::DataBits  recvDataBits;
	SerialDataInterface::StopBits  recvStopBits;
	SerialDataInterface::ParityBit recvParityBit;
	bool                           recvParityEnabled;
	byte                           recvBuf;
	bool recvReady;

	byte sendByte;
	byte sendBuffer;

	byte status;
	byte command;
	byte mode;
	byte sync1, sync2;
};

} // namespace openmsx

#endif
