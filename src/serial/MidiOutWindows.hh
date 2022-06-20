#ifndef MIDIOUTWINDOWS_HH
#define MIDIOUTWINDOWS_HH

#if defined(_WIN32)
#include "MidiOutDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class PluggingController;

class MidiOutWindows final : public MidiOutDevice
{
public:
	static void registerAll(PluggingController& controller);

	explicit MidiOutWindows(unsigned num);
	~MidiOutWindows();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// SerialDataInterface (part)
	void recvMessage(const std::vector<uint8_t>& message, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	unsigned devidx;
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIOUTWINDOWS_HH
