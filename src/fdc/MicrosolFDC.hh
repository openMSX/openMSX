// $Id$

#ifndef __BRAZILFDC_HH__
#define __BRAZILFDC_HH__

#include "WD2793BasedFDC.hh"
#include "MSXIODevice.hh"


class MicrosolFDC : public WD2793BasedFDC, public MSXIODevice
{
	public:
		MicrosolFDC(Device *config, const EmuTime &time);
		virtual ~MicrosolFDC();
		
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		byte driveD4;
};
#endif
