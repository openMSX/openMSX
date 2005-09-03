// $Id$

#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "Scheduler.hh"
#include "FilenameSetting.hh"
#include <cstring>
#include <cerrno>

using std::string;

namespace openmsx {

MidiInReader::MidiInReader(Scheduler& scheduler,
                           CommandController& commandController)
	: Schedulable(scheduler), thread(this), lock(1)
	, readFilenameSetting(new FilenameSetting(
		commandController, "midi-in-readfilename",
		"filename of the file where the MIDI input is read from",
		"/dev/midi"))
{
}

MidiInReader::~MidiInReader()
{
	removeSyncPoint();
}

// Pluggable
void MidiInReader::plugHelper(Connector* connector_, const EmuTime& /*time*/)
{
	file = fopen(readFilenameSetting->getValue().c_str(), "rb");
	if (!file) {
		throw PlugException("Failed to open input: "
			+ string(strerror(errno)));
	}

	MidiInConnector* midiConnector = static_cast<MidiInConnector*>(connector_);
	midiConnector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	midiConnector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	midiConnector->setParityBit(false, SerialDataInterface::EVEN); // no parity

	connector = connector_; // base class will do this in a moment,
	                        // but thread already needs it
	thread.start();
}

void MidiInReader::unplugHelper(const EmuTime& /*time*/)
{
	ScopedLock l(lock);
	thread.stop();
	fclose(file);
}

const string& MidiInReader::getName() const
{
	static const string name("midi-in-reader");
	return name;
}

const string& MidiInReader::getDescription() const
{
	static const string desc(
		"Midi in file reader. Sends data from an input file to the "
		"midi port it is connected to. The filename is set with "
		"the 'midi-in-readfilename' setting.");
	return desc;
}


// Runnable
void MidiInReader::run()
{
	byte buf;
	while (true) {
		int num = fread(&buf, 1, 1, file);
		if (num != 1) {
			continue;
		}
		assert(getConnector());

		ScopedLock l(lock);
		queue.push_back(buf);
		setSyncPoint(Scheduler::ASAP);
	}
}

// MidiInDevice
void MidiInReader::signal(const EmuTime& time)
{
	MidiInConnector* connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}

	ScopedLock l(lock);
	if (queue.empty()) return;
	byte data = queue.front();
	queue.pop_front();
	connector->recvByte(data, time);
}

// Schedulable
void MidiInReader::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (getConnector()) {
		signal(time);
	} else {
		ScopedLock l(lock);
		queue.empty();
	}
}

const string& MidiInReader::schedName() const
{
	return getName();
}

} // namespace openmsx
