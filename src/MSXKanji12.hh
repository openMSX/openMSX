// $Id$

#ifndef __MSXKANJI12_HH__
#define __MSXKANJI12_HH__

#include "MSXDevice.hh"
#include "MSXRomDevice.hh"
#include "MSXDeviceSwitch.hh"


class MSXKanji12 : public MSXDevice, public MSXSwitchedDevice
{
	public:
		MSXKanji12(Device *config, const EmuTime &time);
		virtual ~MSXKanji12();
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		MSXRomDevice rom;
		int adr;
		int mask;
};

#endif //__MSXKANJI12_HH__
