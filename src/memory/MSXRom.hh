// $Id$

#ifndef __MSXRom_HH__
#define __MSXRom_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"


class MSXRom : public MSXMemDevice
{
	public:
		MSXRom(Device* config, const EmuTime &time, Rom *rom);
		virtual ~MSXRom();

		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
		virtual const std::string &getName() const;

	protected:
		Rom* rom;
		std::string romName;
		static class MSXCPU* cpu;

	private:
		void init();
};

#endif
