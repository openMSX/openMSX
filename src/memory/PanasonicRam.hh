// $Id$

#ifndef __PANASONICRAM_HH__
#define __PANASONICRAM_HH__

#include "MSXMemoryMapper.hh"

class PanasonicRam : public MSXMemoryMapper
{
	public:
		PanasonicRam(Device *config, const EmuTime &time);
		virtual ~PanasonicRam();
};

#endif
