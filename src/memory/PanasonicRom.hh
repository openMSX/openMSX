// $Id$

#ifndef __PANASONICROM_HH__
#define __PANASONICROM_HH__

#include "MSXMemDevice.hh"


namespace openmsx {

class PanasonicRom : public MSXMemDevice
{
	public:
		PanasonicRom(Device *config, const EmuTime &time);
		virtual ~PanasonicRom();

		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word address) const;
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
	
	private:
		int block;
};

} // namespace openmsx

#endif
