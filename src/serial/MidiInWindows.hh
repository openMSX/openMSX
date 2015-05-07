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
#include "serialize_meta.hh"
#include "circular_buffer.hh"
#include <windows.h>
#include <mmsystem.h>
#include <mutex>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

class MidiInWindows final : public MidiInDevice, private Runnable
                          , private EventListener
{
public:
	/** Register all available native Windows midi in devices
	  */
	static void registerAll(EventDistributor& eventDistributor,
	                        Scheduler& scheduler,
	                        PluggingController& controller);

	MidiInWindows(EventDistributor& eventDistributor, Scheduler& scheduler,
	             unsigned num);
	~MidiInWindows();

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

	void procShortMsg(long unsigned param);
	void procLongMsg(LPMIDIHDR p);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	unsigned devidx;
	DWORD thrdid;
	cb_queue<byte> queue;
	std::mutex mutex; // to protect queue
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIINWINDOWS_HH
