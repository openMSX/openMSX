// $Id$

// This class implements the Intel 8251 chip (UART)

#ifndef __I8251_HH__
#define __I8251_HH__

#include "openmsx.hh"
#include "EmuTime.hh"
#include "ClockPin.hh"
#include "SerialDataInterface.hh"
#include "Schedulable.hh"


class I8251Interface : public SerialDataInterface
{
	public:
		virtual void setRxRDY(bool status, const EmuTime& time) = 0;
		virtual void setDTR(bool status, const EmuTime& time) = 0;
		virtual void setRTS(bool status, const EmuTime& time) = 0;
		virtual byte getDSR(const EmuTime& time) = 0;
		virtual void signal(const EmuTime& time) = 0;
};

class I8251 : public SerialDataInterface, private Schedulable
{
	public:
		I8251(I8251Interface* interf, const EmuTime &time); 
		virtual ~I8251(); 
		
		void reset(const EmuTime& time);
		byte readIO(byte port, const EmuTime& time);
		void writeIO(byte port, byte value, const EmuTime& time);
		ClockPin& getClockPin();
		bool isRecvReady();
		bool isRecvEnabled();

		// SerialDataInterface
		virtual void setDataBits(DataBits bits);
		virtual void setStopBits(StopBits bits);
		virtual void setParityBit(bool enable, ParityBit parity);
		virtual void recvByte(byte value, const EmuTime& time);

		// Schedulable
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		virtual const string &schedName() const;
	
	private:
		void setMode(byte mode);
		void writeCommand(byte value, const EmuTime& time);
		byte readStatus(const EmuTime& time);
		byte readTrans(const EmuTime& time);
		void writeTrans(byte value, const EmuTime& time);
		void send(byte value, const EmuTime& time);

		I8251Interface* interf;
		enum CmdFaze {
			FAZE_MODE, FAZE_SYNC1, FAZE_SYNC2, FAZE_CMD
		} cmdFaze;
		byte status;
		byte command;
		byte mode;
		byte sync1, sync2;
		ClockPin clock;
		EmuDuration charLength;
		
		SerialDataInterface::DataBits  recvDataBits;
		SerialDataInterface::StopBits  recvStopBits;
		bool                           recvParityEnabled;
		SerialDataInterface::ParityBit recvParityBit;
		byte                           recvBuf;
		bool recvReady;
		
		byte sendByte;
		byte sendBuffer;
		bool sendBuffered;
};

#endif
