// $Id$

#ifndef __WD2793BASEDFDC_HH__
#define __WD2793BASEDFDC_HH__

#include "MSXFDC.hh"

class WD2793;
class DriveMultiplexer;


class WD2793BasedFDC : public MSXFDC
{
	public:
		WD2793BasedFDC(Device *config, const EmuTime &time);
		virtual ~WD2793BasedFDC();
		
		virtual void reset(const EmuTime &time);
		
	protected:
		WD2793* controller;
		DriveMultiplexer* multiplexer;
};

#endif
