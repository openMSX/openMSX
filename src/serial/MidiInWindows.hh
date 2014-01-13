#ifndef MIDIINWINDOWS_HH
#define MIDIINWINDOWS_HH

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
#include "circular_buffer.hh"
#include <SDL_thread.h>
#include <windows.h>
#include <mmsystem.h>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

class MidiInWindows : public MidiInDevice, private Runnable, private EventListener
{
public:
	/** Register all available native Windows midi in devices
	  */
	static void registerAll(EventDistributor& eventDistributor,
	                        Scheduler& scheduler,
	                        PluggingController& controller);

	MidiInWindows(EventDistributor& eventDistributor, Scheduler& scheduler,
	             unsigned num);
	virtual ~MidiInWindows();

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

	void procShortMsg(long unsigned param);
	void procLongMsg(LPMIDIHDR p);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	unsigned devidx;
	unsigned thrdid;
	cb_queue<byte> queue;
	Semaphore lock; // to protect queue
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIINWINDOWS_HH
