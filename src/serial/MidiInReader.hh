#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include "circular_buffer.hh"
#include <cstdio>
#include <memory>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;
class FilenameSetting;

class MidiInReader : public MidiInDevice, private Runnable, private EventListener
{
public:
	MidiInReader(EventDistributor& eventDistributor, Scheduler& scheduler,
	             CommandController& commandController);
	virtual ~MidiInReader();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;

	// MidiInDevice
	virtual void signal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	virtual void run();

	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	FILE* file;
	cb_queue<byte> queue;
	Semaphore lock; // to protect queue

	const std::unique_ptr<FilenameSetting> readFilenameSetting;
};

} // namespace openmsx

#endif
