#ifndef MIDIINWINDOWS_HH
#define MIDIINWINDOWS_HH

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "EventListener.hh"
#include "serialize_meta.hh"
#include "circular_buffer.hh"
#include <windows.h>
#include <mmsystem.h>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

class MidiInWindows final : public MidiInDevice, private EventListener
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
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	int signalEvent(const Event& event) override;

	void procShortMsg(long unsigned param);
	void procLongMsg(LPMIDIHDR p);

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	std::mutex devIdxMutex;
	std::condition_variable devIdxCond;
	unsigned devIdx;
	std::mutex threadIdMutex;
	std::condition_variable threadIdCond;
	DWORD threadId;
	cb_queue<byte> queue;
	std::mutex queueMutex;
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIINWINDOWS_HH
