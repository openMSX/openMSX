// $Id$

#include "MidiInReader.hh"
#include "PluggingController.hh"
#include "MidiInConnector.hh"
#include "Scheduler.hh"


namespace openmsx {

MidiInReader::MidiInReader()
	: thread(this), connector(NULL), lock(1), 
	readFilenameSetting("midi-in-readfilename",
	"filename of the file where the MIDI input is read from",
	"/dev/midi")
{
	PluggingController::instance()->registerPluggable(this);
}

MidiInReader::~MidiInReader()
{
	Scheduler::instance()->removeSyncPoint(this);
	PluggingController::instance()->unregisterPluggable(this);
}

// Pluggable
void MidiInReader::plug(Connector* connector_, const EmuTime& time)
{
	file = fopen(readFilenameSetting.getValue().c_str(), "rb");
	if (!file) {
		return;
	}
	
	connector = (MidiInConnector*)connector_;
	connector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	connector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	connector->setParityBit(false, SerialDataInterface::EVEN); // no parity
	
	thread.start();
}

void MidiInReader::unplug(const EmuTime& time)
{
	lock.down();
	thread.stop();
	lock.up();
	connector = NULL;
	fclose(file);
}

const string& MidiInReader::getName() const
{
	static const string name("midi-in-reader");
	return name;
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
		assert(connector);
		lock.down();
		queue.push_back(buf);
		Scheduler::instance()->setSyncPoint(Scheduler::ASAP, this);
		lock.up();
	}
}

// MidiInDevice
void MidiInReader::signal(const EmuTime& time)
{
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
void MidiInReader::executeUntilEmuTime(const EmuTime& time, int userData)
{
	if (connector) {
		signal(time);
	} else {
		lock.down();
		queue.empty();
		lock.up();
	}
}

const string& MidiInReader::schedName() const
{
	return getName();
}

} // namespace openmsx
