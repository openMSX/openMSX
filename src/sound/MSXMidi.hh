// $Id$

#ifndef __MSXMIDI_HH__
#define __MSXMIDI_HH__

#include "MSXIODevice.hh"


class MSXMidi : public MSXIODevice
{
	public:
		MSXMidi(Device *config, const EmuTime &time);
		virtual ~MSXMidi();

		virtual void powerOff(const EmuTime &time);
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
};

#endif
