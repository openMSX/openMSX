// $Id$

#ifndef __MSXYM2413_HH__
#define __MSXYM2413_HH__

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"

class YM2413;


class MSXYM2413 : public MSXIODevice, public MSXMemDevice
{
	public:
		MSXYM2413(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXYM2413(); 
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	protected:
		void writeRegisterPort(byte value, const EmuTime &time);
		void writeDataPort(byte value, const EmuTime &time);

		byte enable;
		MSXRomDevice rom;
		YM2413 *ym2413;
		
	private:
		int registerLatch;
};

#endif
