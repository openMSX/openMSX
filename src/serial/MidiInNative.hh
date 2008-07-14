// $Id$

#ifndef MIDIINNATIVE_HH
#define MIDIINNATIVE_HH

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "serialize_meta.hh"
#include <SDL_thread.h>
#include <windows.h>
#include <mmsystem.h>
#include <deque>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

class MidiInNative : public MidiInDevice, private Runnable, private EventListener
{
public:
	/** Register all available native midi in devcies
	  */
	static void registerAll(EventDistributor& eventDistributor,
	                        Scheduler& scheduler,
	                        PluggingController& controller);

	MidiInNative(EventDistributor& eventDistributor, Scheduler& scheduler,
	             unsigned num);
	virtual ~MidiInNative();

	// Pluggable
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// MidiInDevice
	virtual void signal(const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	virtual void run();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	void procShortMsg(long unsigned param);
	void procLongMsg(LPMIDIHDR p);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	unsigned devidx;
	unsigned thrdid;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue
	std::string name;
	std::string desc;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInNative, "MidiInNative");

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIINNATIVE_HH

