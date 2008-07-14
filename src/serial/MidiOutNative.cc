// $Id$

#if defined(_WIN32)

#include "Midi_w32.hh"
#include "MidiOutNative.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"

using std::string;

namespace openmsx {

void MidiOutNative::registerAll(PluggingController& controller)
{
	w32_midiOutInit();
	unsigned devnum = w32_midiOutGetVFNsNum();
	for (unsigned i = 0; i < devnum; ++i) {
		controller.registerPluggable(new MidiOutNative(i));
	}
}


MidiOutNative::MidiOutNative(unsigned num)
	: devidx(unsigned(-1))
{
	name = w32_midiOutGetVFN(num);
	desc = w32_midiOutGetRDN(num);
}

MidiOutNative::~MidiOutNative()
{
	//w32_midiOutClean(); // TODO
}

void MidiOutNative::plugHelper(Connector& connector, const EmuTime& time)
{
	devidx = w32_midiOutOpen(name.c_str());
	if (devidx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiOutNative::unplugHelper(const EmuTime& time)
{
	if (devidx != unsigned(-1)) {
		w32_midiOutClose(devidx);
		devidx = unsigned(-1);
	}
}

const string& MidiOutNative::getName() const
{
	return name;
}

const string& MidiOutNative::getDescription() const
{
	return desc;
}

void MidiOutNative::recvByte(byte value, const EmuTime& time)
{
	if (devidx != unsigned(-1)) {
		w32_midiOutPut(value, devidx);
	}
}

template<typename Archive>
void MidiOutNative::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't restore this after loadstate
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutNative);

} // namespace openmsx

#endif // defined(_WIN32)
