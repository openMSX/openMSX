#ifndef MIDIINWINDOWS_HH
#define MIDIINWINDOWS_HH

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "MidiInDevice.hh"
#include "EventListener.hh"
#include "serialize_meta.hh"
#include "circular_buffer.hh"
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <latch>
#include <mutex>
#include <optional>
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
	~MidiInWindows() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	bool signalEvent(const Event& event) override;

	void procShortMsg(long unsigned param);
	void procLongMsg(LPMIDIHDR p);

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	std::optional<std::latch> threadIdLatch;
	std::optional<std::latch> devIdxLatch;
	unsigned devIdx = unsigned(-1);
	DWORD threadId;
	cb_queue<uint8_t> queue;
	std::mutex queueMutex;
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIINWINDOWS_HH
