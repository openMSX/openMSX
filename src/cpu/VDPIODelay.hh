// $Id$

#ifndef __VDPIODELAY_HH__
#define __VDPIODELAY_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"

class MSXCPU;


class VDPIODelay : public MSXIODevice
{
	public:
		VDPIODelay(MSXIODevice *device, const EmuTime &time);

		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		const EmuTime &delay(const EmuTime &time);

		MSXCPU *cpu;
		MSXIODevice *device;
		EmuTimeFreq<7159090> lastTime;
};

#endif
