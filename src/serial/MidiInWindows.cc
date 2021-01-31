#if defined(_WIN32)
#include "MidiInWindows.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cstring>
#include <cerrno>
#include "Midi_w32.hh"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>

using std::string;

namespace openmsx {

void MidiInWindows::registerAll(EventDistributor& eventDistributor,
                                Scheduler& scheduler,
                                PluggingController& controller)
{
	w32_midiInInit();
	for (auto i : xrange(w32_midiInGetVFNsNum())) {
		controller.registerPluggable(std::make_unique<MidiInWindows>(
			eventDistributor, scheduler, i));
	}
}


MidiInWindows::MidiInWindows(EventDistributor& eventDistributor_,
                             Scheduler& scheduler_, unsigned num)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, devIdx(unsigned(-1))
{
	name = w32_midiInGetVFN(num);
	desc = w32_midiInGetRDN(num);

	eventDistributor.registerEventListener(OPENMSX_MIDI_IN_WINDOWS_EVENT, *this);
}

MidiInWindows::~MidiInWindows()
{
	eventDistributor.unregisterEventListener(OPENMSX_MIDI_IN_WINDOWS_EVENT, *this);

	//w32_midiInClean(); // TODO
}

// Pluggable
void MidiInWindows::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	auto& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it

	{
		std::unique_lock<std::mutex> threadIdLock(threadIdMutex);
		thread = std::thread([this]() { run(); });
		threadIdCond.wait(threadIdLock);
	}
	{
		std::lock_guard<std::mutex> devIdxLock(devIdxMutex);
		devIdx = w32_midiInOpen(name.c_str(), threadId);
	}
	devIdxCond.notify_all();
	if (devIdx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}
}

void MidiInWindows::unplugHelper(EmuTime::param /*time*/)
{
	assert(devIdx != unsigned(-1));
	w32_midiInClose(devIdx);
	devIdx = unsigned(-1);
	thread.join();
}

std::string_view MidiInWindows::getName() const
{
	return name;
}

std::string_view MidiInWindows::getDescription() const
{
	return desc;
}

void MidiInWindows::procLongMsg(LPMIDIHDR p)
{
	if (p->dwBytesRecorded) {
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			for (auto i : xrange(p->dwBytesRecorded)) {
				queue.push_back(p->lpData[i]);
			}
		}
		eventDistributor.distributeEvent(
			std::make_shared<SimpleEvent>(OPENMSX_MIDI_IN_WINDOWS_EVENT));
	}
}

void MidiInWindows::procShortMsg(DWORD param)
{
	int num;
	switch (param & 0xF0) {
		case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0:
			num = 3; break;
		case 0xC0: case 0xD0:
			num = 2; break;
		default:
			num = 1; break;
	}
	std::lock_guard<std::mutex> lock(queueMutex);
	while (num--) {
		queue.push_back(param & 0xFF);
		param >>= 8;
	}
	eventDistributor.distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_MIDI_IN_WINDOWS_EVENT));
}

void MidiInWindows::run()
{
	assert(isPluggedIn());

	{
		std::lock_guard<std::mutex> threadIdLock(threadIdMutex);
		threadId = GetCurrentThreadId();
	}

	{
		std::unique_lock<std::mutex> devIdxLock(devIdxMutex);
		threadIdCond.notify_all();
		devIdxCond.wait(devIdxLock);
	}

	bool fexit = devIdx == unsigned(-1);
	while (!fexit) {
		MSG msg;
		if (GetMessage(&msg, nullptr, 0, 0) == one_of(0, -1)) {
			break;
		}
		switch (msg.message) {
			case MM_MIM_OPEN:
				break;
			case MM_MIM_DATA:
			case MM_MIM_MOREDATA:
				procShortMsg(DWORD(msg.lParam));
				break;
			case MM_MIM_LONGDATA:
				procLongMsg(reinterpret_cast<LPMIDIHDR>(msg.lParam));
				break;
			case MM_MIM_ERROR:
			case MM_MIM_LONGERROR:
			case MM_MIM_CLOSE:
				fexit = true;
				break;
			default:
				break;
		}
	}
	threadId = 0;
}

// MidiInDevice
void MidiInWindows::signal(EmuTime::param time)
{
	auto* conn = static_cast<MidiInConnector*>(getConnector());
	if (!conn->acceptsData()) {
		std::lock_guard<std::mutex> lock(queueMutex);
		queue.clear();
		return;
	}
	if (!conn->ready()) return;

	byte data;
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	conn->recvByte(data, time);
}

// EventListener
int MidiInWindows::signalEvent(const std::shared_ptr<const Event>& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard<std::mutex> lock(queueMutex);
		queue.clear();
	}
	return 0;
}

template<typename Archive>
void MidiInWindows::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't restore this after loadstate
}
INSTANTIATE_SERIALIZE_METHODS(MidiInWindows);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInWindows, "MidiInWindows");

} // namespace openmsx

#endif // defined(_WIN32)
