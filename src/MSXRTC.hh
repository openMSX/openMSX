// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include "MSXIODevice.hh"
#include "SRAM.hh"

class RP5C01;


class MSXRTC : public MSXIODevice
{
	public:
		MSXRTC(Device *config, const EmuTime &time);
		virtual ~MSXRTC(); 

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		RP5C01 *rp5c01;
		SRAM sram;
		nibble registerLatch;
};

#endif
