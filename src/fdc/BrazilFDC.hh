// $Id$

#ifndef __BRAZILFDC_HH__
#define __BRAZILFDC_HH__

#include "MSXFDC.hh"
#include "MSXIODevice.hh"

class WD2793;


class BrazilFDC : public MSXFDC, public MSXIODevice
{
	public:
		BrazilFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~BrazilFDC();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		WD2793* controller;
		bool brokenFDCread;
		byte driveD4;
};
#endif
