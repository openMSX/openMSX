#ifndef __MSXSIMPLE_HH__
#define __MSXSIMPLE_HH__

#include "MSXIODevice.hh"
class DACSound;

class MSXSimple : public MSXIODevice
{
	public:
		MSXSimple(MSXConfig::Device *config, const EmuTime &time);
		~MSXSimple();

		void reset(const EmuTime &time);

		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);

	private:
		DACSound* dac;
};
#endif

