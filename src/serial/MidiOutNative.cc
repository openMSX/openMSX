// $Id$

#if defined(__WIN32__)

#include "Midi_w32.hh"
#include "MidiOutNative.hh"
#include "PluggingController.hh"

namespace openmsx {

void MidiOutNative::registerAll(PluggingController* controller)
{
	w32_midiOutInit();
	unsigned devnum = w32_midiOutGetVFNsNum();
	for (unsigned i = 0; i < devnum; ++i) {
		char buf[MAXPATHLEN + 1];
		w32_midiOutGetVFN(buf, i);
		if (buf[0] != '\0') {
			controller->registerPluggable(new MidiOutNative(buf));
		}
	}
}


MidiOutNative::MidiOutNative(const string& name_)
	: name(name_)
{
}

MidiOutNative::~MidiOutNative()
{
	//w32_midiOutClean(); // TODO
}

void MidiOutNative::plug(Connector *connector, const EmuTime &time)
	throw(PlugException)
{
	devidx = w32_midiOutOpen(name.c_str());
	if (devidx == (unsigned)-1) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiOutNative::unplug(const EmuTime &time)
{
	w32_midiOutClose(devidx);
}

const string& MidiOutNative::getName() const
{
	return name;
}

void MidiOutNative::recvByte(byte value, const EmuTime &time)
{
	w32_midiOutPut(value, devidx);
}

} // namespace openmsx

#endif // defined(__WIN32__)
