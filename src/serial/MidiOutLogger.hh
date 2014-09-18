#ifndef MIDIOUTLOGGER_HH
#define MIDIOUTLOGGER_HH

#include "MidiOutDevice.hh"
#include <fstream>
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;

class MidiOutLogger final : public MidiOutDevice
{
public:
	explicit MidiOutLogger(CommandController& commandController);
	~MidiOutLogger();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<FilenameSetting> logFilenameSetting;
	std::ofstream file;
};

} // namespace openmsx

#endif
