// $Id$

#ifndef __WD2793BASEDFDC_HH__
#define __WD2793BASEDFDC_HH__

#include "MSXFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"


class WD2793BasedFDC : public MSXFDC
{
	public:
		WD2793BasedFDC(Device *config, const EmuTime &time);
		virtual ~WD2793BasedFDC() = 0;
		
		virtual void reset(const EmuTime &time);
		
	protected:
		DriveMultiplexer multiplexer;
		WD2793 controller;
};

#endif
