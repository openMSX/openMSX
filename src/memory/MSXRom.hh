// $Id$

#ifndef __MSXRom_HH__
#define __MSXRom_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"
#include <string>


namespace openmsx {

class MSXRom : public MSXMemDevice
{
	public:
		virtual ~MSXRom();

		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte *getWriteCacheLine(word address) const;
		virtual const string &getName() const;

	protected:
		MSXRom(Device *config, const EmuTime &time, Rom *rom);

		Rom* rom;
		string romName;
		static class MSXCPU *cpu;

	private:
		void init();
};

} // namespace openmsx

#endif
