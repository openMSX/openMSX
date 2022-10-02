#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "EventListener.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "circular_buffer.hh"
#include "Poller.hh"
#include <cstdint>
#include <mutex>
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
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	int signalEvent(const Event& event) noexcept override;

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	FileOperations::FILE_t file;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue
	Poller poller;

	FilenameSetting readFilenameSetting;
};

} // namespace openmsx

#endif
