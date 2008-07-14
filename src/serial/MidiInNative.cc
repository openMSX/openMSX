// $Id$

#if defined(_WIN32)
#include "MidiInNative.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "serialize.hh"
#include <cstring>
#include <cerrno>
#include "Midi_w32.hh"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

using std::string;

namespace openmsx {

void MidiInNative::registerAll(EventDistributor& eventDistributor,
                               Scheduler& scheduler,
                               PluggingController& controller)
{
	w32_midiInInit();
	unsigned devnum = w32_midiInGetVFNsNum();
	for (unsigned i = 0 ; i <devnum; ++i) {
		controller.registerPluggable(
			new MidiInNative(eventDistributor, scheduler, i));
	}
}


MidiInNative::MidiInNative(EventDistributor& eventDistributor_,
                           Scheduler& scheduler_, unsigned num)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, thread(this), devidx(unsigned(-1)), lock(1)
{
	name = w32_midiInGetVFN(num);
	desc = w32_midiInGetRDN(num);

	eventDistributor.registerEventListener(OPENMSX_MIDI_IN_NATIVE_EVENT, *this);
}

MidiInNative::~MidiInNative()
{
	eventDistributor.unregisterEventListener(OPENMSX_MIDI_IN_NATIVE_EVENT, *this);

	//w32_midiInClean(); // TODO
}

// Pluggable
void MidiInNative::plugHelper(Connector& connector_, const EmuTime& time)
{
	devidx = w32_midiInOpen(name.c_str(), thrdid);
	if (devidx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}

	MidiInConnector& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	connector = &connector_; // base class will do this in a moment,
	                         // but thread already needs it
	thread.start();
}

void MidiInNative::unplugHelper(const EmuTime& time)
{
	ScopedLock l(lock);
	thread.stop();
	if (devidx != unsigned(-1)) {
		w32_midiInClose(devidx);
		devidx = unsigned(-1);
	}
}

const string& MidiInNative::getName() const
{
	return name;
}

const string& MidiInNative::getDescription() const
{
	return desc;
}

void MidiInNative::procLongMsg(LPMIDIHDR p)
{
	if (p->dwBytesRecorded) {
		ScopedLock l(lock);
		for (unsigned i = 0; i < p->dwBytesRecorded; ++i) {
			queue.push_back(p->lpData[i]);
		}
		eventDistributor.distributeEvent(
			new SimpleEvent<OPENMSX_MIDI_IN_NATIVE_EVENT>());
	}
}

void MidiInNative::procShortMsg(DWORD param)
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
	ScopedLock l(lock);
	while (num--) {
		queue.push_back(param & 0xFF);
		param >>= 8;
	}
	eventDistributor.distributeEvent(
		new SimpleEvent<OPENMSX_MIDI_IN_NATIVE_EVENT>());
}

// Runnable
void MidiInNative::run()
{
	assert(getConnector());
	thrdid = SDL_ThreadID();

	MSG msg;
	bool fexit = false;
	int gmer;
	while (!fexit && (gmer = GetMessage(&msg, NULL, 0, 0))) {
		if (gmer == -1) {
			break;
		}
		switch (msg.message) {
			case MM_MIM_OPEN:
				break;
			case MM_MIM_DATA:
			case MM_MIM_MOREDATA:
				procShortMsg(msg.lParam);
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
	thrdid = 0;
}

// MidiInDevice
void MidiInNative::signal(const EmuTime& time)
{
	MidiInConnector* connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		queue.clear();
		return;
	}
	if (!connector->ready()) return;

	ScopedLock l(lock);
	if (queue.empty()) return;
	byte data = queue.front();
	queue.pop_front();
	connector->recvByte(data, time);
}

// EventListener
bool MidiInNative::signalEvent(shared_ptr<const Event> /*event*/)
{
	if (getConnector()) {
		signal(scheduler.getCurrentTime());
	} else {
		ScopedLock l(lock);
		queue.empty();
	}
	return true;
}

template<typename Archive>
void MidiInNative::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't restore this after loadstate
}
INSTANTIATE_SERIALIZE_METHODS(MidiInNative);

} // namespace openmsx

#endif // defined(_WIN32)
