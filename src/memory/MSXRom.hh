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

	protected:
		Rom rom;
		static const int UNMAPPED_SIZE = 0x8000;
		static byte unmapped[UNMAPPED_SIZE];
		static class MSXCPU* cpu;

	private:
		void init();
};

#endif
