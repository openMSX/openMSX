// $Id$

#ifndef __MSXMatsushita_HH__
#define __MSXMatsushita_HH__

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "FrontSwitch.hh"
#include "SRAM.hh"


namespace openmsx {

class MSXMatsushita : public MSXDevice, public MSXSwitchedDevice
{
	public:
		MSXMatsushita(Device *config, const EmuTime &time);
		virtual ~MSXMatsushita();

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		FrontSwitch frontSwitch;
		SRAM sram;
		word address;

		nibble color1, color2;
		byte pattern;
};

} // namespace openmsx

#endif
