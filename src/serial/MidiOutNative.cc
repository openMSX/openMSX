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
		controller->registerPluggable(new MidiOutNative(i));
	}
}


MidiOutNative::MidiOutNative(unsigned num)
{
	name = w32_midiOutGetVFN(num);
	desc = w32_midiOutGetRDN(num);
}

MidiOutNative::~MidiOutNative()
{
	//w32_midiOutClean(); // TODO
}

void MidiOutNative::plugHelper(Connector *connector, const EmuTime &time)
	throw(PlugException)
{
	devidx = w32_midiOutOpen(name.c_str());
	if (devidx == (unsigned)-1) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiOutNative::unplugHelper(const EmuTime &time)
{
	w32_midiOutClose(devidx);
}

const string& MidiOutNative::getName() const
{
	return name;
}

const string& MidiOutNative::getDescription() const
{
	return desc;
}

void MidiOutNative::recvByte(byte value, const EmuTime &time)
{
	w32_midiOutPut(value, devidx);
}

} // namespace openmsx

#endif // defined(__WIN32__)
