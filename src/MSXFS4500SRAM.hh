// $Id$

#ifndef __MSXFS4500SRAM_HH__
#define __MSXFS4500SRAM_HH__

#include "MSXIODevice.hh"
#include "SRAM.hh"


class MSXFS4500SRAM : public MSXIODevice
{
	public:
		MSXFS4500SRAM(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXFS4500SRAM();

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		SRAM sram;
		int address;
};

#endif
