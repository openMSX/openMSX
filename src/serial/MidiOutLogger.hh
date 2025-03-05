#ifndef MIDIOUTLOGGER_HH
#define MIDIOUTLOGGER_HH

#include "MidiOutDevice.hh"
#include "FilenameSetting.hh"

#include <fstream>

namespace openmsx {

class CommandController;

class MidiOutLogger final : public MidiOutDevice
{
public:
	explicit MidiOutLogger(CommandController& commandController);

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// SerialDataInterface (part)
	void recvByte(uint8_t value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	FilenameSetting logFilenameSetting;
	std::ofstream file;
};

} // namespace openmsx

#endif
