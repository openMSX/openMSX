// $Id$

#ifndef __MC6850_HH__
#define __MC6850_HH__

#include "MSXIODevice.hh"


class MC6850 : public MSXIODevice
{
	public:
		MC6850(MSXConfig::Device *config, const EmuTime &time);
		~MC6850(); 
		
		void reset(const EmuTime &time);
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
};
#endif
