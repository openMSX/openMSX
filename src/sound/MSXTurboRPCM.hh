// $Id$

#ifndef __MSXTURBORPCM_HH__
#define __MSXTURBORPCM_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"

class DACSound8U;


class MSXTurboRPCM : public MSXIODevice
{
	public:
		MSXTurboRPCM(Device *config, const EmuTime &time);
		virtual ~MSXTurboRPCM(); 
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		byte readSample();
		byte getSample();
		bool getComp();
		
		EmuTimeFreq<15750> reference;
		byte DValue;
		byte status;
		byte hold;
		DACSound8U* dac;
};

#endif
