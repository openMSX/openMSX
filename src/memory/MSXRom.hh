// $Id$

#ifndef __MSXRom_HH__
#define __MSXRom_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"


class MSXRom : public MSXMemDevice
{
	public:
		MSXRom(Device* config, const EmuTime &time);
		virtual ~MSXRom();

		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;

	protected:
		Rom rom;
		static class MSXCPU* cpu;

	private:
		void init();
};

#endif
