// $Id$

#if defined(_WIN32)
#include "MidiInNative.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "Scheduler.hh"
#include <cstring>
#include <cerrno>
#include "FileOperations.hh"
#include "FileContext.hh"
#include "Midi_w32.hh"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace openmsx {

void MidiInNative::registerAll(PluggingController* controller)
{
	w32_midiInInit();
	unsigned devnum = w32_midiInGetVFNsNum();
	for (unsigned i = 0 ; i <devnum; ++i) {
		controller->registerPluggable(new MidiInNative(i));
	}
}


MidiInNative::MidiInNative(unsigned num)
	: thread(this), lock(1)
{
	name = w32_midiInGetVFN(num);
	desc = w32_midiInGetRDN(num);
}

MidiInNative::~MidiInNative()
{
	Scheduler::instance().removeSyncPoint(this);
	//w32_midiInClean(); // TODO
}

// Pluggable
void MidiInNative::plugHelper(Connector* connector_, const EmuTime& time)
{
	devidx = w32_midiInOpen(name.c_str(), thrdid);
	if (devidx == (unsigned)-1) {
		throw PlugException("Failed to open " + name);
	}

	MidiInConnector* midiConnector = static_cast<MidiInConnector*>(connector_);
	midiConnector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	midiConnector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	midiConnector->setParityBit(false, SerialDataInterface::EVEN); // no parity

	connector = connector_; // base class will do this in a moment,
	                        // but thread already needs it
	thread.start();
}

void MidiInNative::unplugHelper(const EmuTime& time)
{
	lock.down();
	thread.stop();
	lock.up();
	w32_midiInClose(devidx);
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
		lock.down();
		for (unsigned i = 0; i < p->dwBytesRecorded; ++i) {
			queue.push_back(p->lpData[i]);
		}
		Scheduler::instance().setSyncPoint(Scheduler::ASAP, this);
		lock.up();
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
	lock.down();
	while (num--) {
		queue.push_back(param & 0xFF);
		param >>= 8;
	}
	Scheduler::instance().setSyncPoint(Scheduler::ASAP,this);
	lock.up();
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
				procLongMsg((LPMIDIHDR)msg.lParam);
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
	thrdid = (unsigned)NULL;
}

// MidiInDevice
void MidiInNative::signal(const EmuTime& time)
{
	MidiInConnector* connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}
	lock.down();
	if (queue.empty()) {
		lock.up();
		return;
	}
	byte data = queue.front();
	queue.pop_front();
	lock.up();
	connector->recvByte(data, time);
}

// Schedulable
void MidiInNative::executeUntil(const EmuTime& time, int userData)
{
	if (getConnector()) {
		signal(time);
	} else {
		lock.down();
		queue.empty();
		lock.up();
	}
}

const string &MidiInNative::schedName() const
{
	return getName();
}

} // namespace openmsx

#endif // defined(_WIN32)
