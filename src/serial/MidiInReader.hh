#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "openmsx.hh"
#include "circular_buffer.hh"
#include <cstdio>
#include <mutex>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class MidiInReader final : public MidiInDevice, private Runnable
                         , private EventListener
{
public:
	MidiInReader(EventDistributor& eventDistributor, Scheduler& scheduler,
	             CommandController& commandController);
	~MidiInReader();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	const std::string& getName() const override;
	string_ref getDescription() const override;

	// MidiInDevice
	void signal(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	void run() override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	FileOperations::FILE_t file;
	cb_queue<byte> queue;
	std::mutex mutex; // to protect queue

	FilenameSetting readFilenameSetting;
};

} // namespace openmsx

#endif
