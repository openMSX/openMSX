#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"

#include "EventListener.hh"
#include "FileOperations.hh"
#include "FilenameSetting.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class MidiInReader final : public MidiInDevice, private EventListener
{
public:
	MidiInReader(EventDistributor& eventDistributor, Scheduler& scheduler,
	             CommandController& commandController);
	~MidiInReader() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	bool signalEvent(const Event& event) override;

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	FileOperations::FILE_t file;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue
	std::optional<Poller> poller;

	FilenameSetting readFilenameSetting;
};

} // namespace openmsx

#endif
