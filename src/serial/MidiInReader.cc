// $Id$

#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <cstdio>
#include <cerrno>
#include <cstring>

using std::string;

namespace openmsx {

MidiInReader::MidiInReader(EventDistributor& eventDistributor_,
                           Scheduler& scheduler_,
                           CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, thread(this), file(NULL), lock(1)
	, readFilenameSetting(new FilenameSetting(
		commandController, "midi-in-readfilename",
		"filename of the file where the MIDI input is read from",
		"/dev/midi"))
{
	eventDistributor.registerEventListener(OPENMSX_MIDI_IN_READER_EVENT, *this);
}

MidiInReader::~MidiInReader()
{
	eventDistributor.unregisterEventListener(OPENMSX_MIDI_IN_READER_EVENT, *this);
}

// Pluggable
void MidiInReader::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	file = FileOperations::openFile(readFilenameSetting->getValue(), "rb");
	if (!file) {
		throw PlugException(StringOp::Builder()
			<< "Failed to open input: " << strerror(errno));
	}

	MidiInConnector& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread.start();
}

void MidiInReader::unplugHelper(EmuTime::param /*time*/)
{
	ScopedLock l(lock);
	thread.stop();
	if (file) {
		fclose(file);
		file = NULL;
	}
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
	if (!file) return;
	while (true) {
		size_t num = fread(&buf, 1, 1, file);
		if (num != 1) {
			continue;
		}
		assert(isPluggedIn());

		ScopedLock l(lock);
		queue.push_back(buf);
		eventDistributor.distributeEvent(
			new SimpleEvent(OPENMSX_MIDI_IN_READER_EVENT));
	}
}

// MidiInDevice
void MidiInReader::signal(EmuTime::param time)
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

// EventListener
int MidiInReader::signalEvent(shared_ptr<const Event> /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		ScopedLock l(lock);
		queue.empty();
	}
	return 0;
}


template<typename Archive>
void MidiInReader::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(MidiInReader);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInReader, "MidiInReader");

} // namespace openmsx
