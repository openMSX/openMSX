// $Id$

#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <cstdio>
#include <deque>
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
	virtual const std::string& getDescription() const;

	// MidiInDevice
	virtual void signal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	virtual void run();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	FILE* file;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue

	const std::auto_ptr<FilenameSetting> readFilenameSetting;
};

} // namespace openmsx

#endif
