// $Id$

#ifndef __MSXMatsushita_HH__
#define __MSXMatsushita_HH__

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "SRAM.hh"


class MSXMatsushita : public MSXDevice, public MSXSwitchedDevice
{
	public:
		MSXMatsushita(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXMatsushita();

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		SRAM sram;
		word address;

		nibble color1, color2;
		byte pattern;
};

#endif
