// $Id$

#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "Scheduler.hh"
#include <cstring>
#include <cerrno>


namespace openmsx {

MidiInReader::MidiInReader()
	: thread(this), lock(1)
	, readFilenameSetting("midi-in-readfilename",
		"filename of the file where the MIDI input is read from",
		"/dev/midi")
{
}

MidiInReader::~MidiInReader()
{
	Scheduler::instance().removeSyncPoint(this);
}

// Pluggable
void MidiInReader::plugHelper(Connector *connector_, const EmuTime &time)
	throw(PlugException)
{
	file = fopen(readFilenameSetting.getValue().c_str(), "rb");
	if (!file) {
		throw PlugException("Failed to open input: "
			+ string(strerror(errno)) );
	}

	MidiInConnector* connector = static_cast<MidiInConnector*>(connector_);
	connector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	connector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	connector->setParityBit(false, SerialDataInterface::EVEN); // no parity

	thread.start();
}

void MidiInReader::unplugHelper(const EmuTime &time)
{
	lock.down();
	thread.stop();
	lock.up();
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
		lock.down();
		queue.push_back(buf);
		Scheduler::instance().setAsyncPoint(this);
		lock.up();
	}
}

// MidiInDevice
void MidiInReader::signal(const EmuTime &time)
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
void MidiInReader::executeUntil(const EmuTime& time, int userData) throw()
{
	if (getConnector()) {
		signal(time);
	} else {
		lock.down();
		queue.empty();
		lock.up();
	}
}

const string &MidiInReader::schedName() const
{
	return getName();
}

} // namespace openmsx
