// $Id$

#ifndef __MSXPAC_HH__
#define __MSXPAC_HH__

#include "MSXMemDevice.hh"
#include "SRAM.hh"


namespace openmsx {

class MSXPac : public MSXMemDevice
{
	public:
		MSXPac(Device *config, const EmuTime &time);
		virtual ~MSXPac(); 
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual const byte* getReadCacheLine(word address) const;
		virtual byte* getWriteCacheLine(word address) const;

	private:
		void checkSramEnable();
		
		bool sramEnabled;
		byte r1ffe, r1fff;
		SRAM sram;
};

} // namespace openmsx

#endif
