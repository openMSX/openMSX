// $Id$

#ifndef __MSXTURBORPCM_HH__
#define __MSXTURBORPCM_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "AudioInputConnector.hh"

class DACSound8U;


class MSXTurboRPCM : public MSXIODevice, private AudioInputConnector
{
	public:
		MSXTurboRPCM(Device *config, const EmuTime &time);
		virtual ~MSXTurboRPCM(); 
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		byte getSample(const EmuTime &time);
		bool getComp(const EmuTime &time);
		
		// AudioInputConnector
		virtual const string &getName() const;

		EmuTimeFreq<15750> reference;
		byte DValue;
		byte status;
		byte hold;
		DACSound8U* dac;
};

#endif
