// $Id$

/*
 * This class implements the MSX-engine found in a MSX Turbo-R (S1990)
 * 
 *  TODO explanation  
 */

#ifndef __S1990_HH__
#define __S1990_HH__

#include "MSXIODevice.hh"
#include "FrontSwitch.hh"


class MSXS1990 : public MSXIODevice
{
	public:
		MSXS1990(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXS1990();
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		void setCPUStatus(byte value);

		FrontSwitch frontSwitch;
		byte registerSelect;
		byte cpuStatus;
};

#endif
