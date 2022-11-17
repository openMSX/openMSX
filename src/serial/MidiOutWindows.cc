#if defined(_WIN32)

#include "Midi_w32.hh"
#include "MidiOutWindows.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

void MidiOutWindows::registerAll(PluggingController& controller)
{
	w32_midiOutInit();
	for (auto i : xrange(w32_midiOutGetVFNsNum())) {
		controller.registerPluggable(std::make_unique<MidiOutWindows>(i));
	}
}


MidiOutWindows::MidiOutWindows(unsigned num)
	: devIdx(unsigned(-1))
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
	devIdx = w32_midiOutOpen(name.c_str());
	if (devIdx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiOutWindows::unplugHelper(EmuTime::param /*time*/)
{
	if (devIdx != unsigned(-1)) {
		w32_midiOutClose(devIdx);
		devIdx = unsigned(-1);
	}
}

std::string_view MidiOutWindows::getName() const
{
	return name;
}

std::string_view MidiOutWindows::getDescription() const
{
	return desc;
}

void MidiOutWindows::recvMessage(const std::vector<uint8_t>& message, EmuTime::param /*time*/)
{
	if (devIdx != unsigned(-1)) {
		w32_midiOutMsg(message.size(), message.data(), devIdx);
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
