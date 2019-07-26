#if defined(_WIN32)

#include "Midi_w32.hh"
#include "MidiOutWindows.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include <memory>

using std::string;

namespace openmsx {

void MidiOutWindows::registerAll(PluggingController& controller)
{
	w32_midiOutInit();
	unsigned devnum = w32_midiOutGetVFNsNum();
	for (unsigned i = 0; i < devnum; ++i) {
		controller.registerPluggable(std::make_unique<MidiOutWindows>(i));
	}
}


MidiOutWindows::MidiOutWindows(unsigned num)
	: devidx(unsigned(-1))
{
	name = w32_midiOutGetVFN(num);
	desc = w32_midiOutGetRDN(num);
}

MidiOutWindows::~MidiOutWindows()
{
	//w32_midiOutClean(); // TODO
}

void MidiOutWindows::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	devidx = w32_midiOutOpen(name.c_str());
	if (devidx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiOutWindows::unplugHelper(EmuTime::param /*time*/)
{
	if (devidx != unsigned(-1)) {
		w32_midiOutClose(devidx);
		devidx = unsigned(-1);
	}
}

const string& MidiOutWindows::getName() const
{
	return name;
}

std::string_view MidiOutWindows::getDescription() const
{
	return desc;
}

void MidiOutWindows::recvByte(byte value, EmuTime::param /*time*/)
{
	if (devidx != unsigned(-1)) {
		w32_midiOutPut(value, devidx);
	}
}

template<typename Archive>
void MidiOutWindows::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't restore this after loadstate
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutWindows);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutWindows, "MidiOutWindows");

} // namespace openmsx

#endif // defined(_WIN32)
