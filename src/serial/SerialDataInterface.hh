// $Id$

#ifndef __SERIALDATAINTERFACE_HH__
#define __SERIALDATAINTERFACE_HH__

#include "openmsx.hh"

class EmuTime;

class SerialDataInterface
{
	public:
		enum DataBits {
			DATA_5 = 5, DATA_6 = 6, DATA_7 = 7, DATA_8 = 8
		};
		enum StopBits {
			STOP_INV = 0, STOP_1 = 2, STOP_15 = 3, STOP_2 = 4
		};

		enum ParityBit {
			EVEN = 0, ODD = 1
		};
		
		virtual void setDataBits(DataBits bits) = 0;
		virtual void setStopBits(StopBits bits) = 0;
		virtual void setParityBits(bool enable, ParityBit parity) = 0;
		virtual void recvByte(byte value, const EmuTime& time) = 0;
};

#endif
