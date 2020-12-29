#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
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
	, readFilenameSetting(
		commandController, "midi-in-readfilename",
		"filename of the file where the MIDI input is read from",
		"/dev/midi")
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
	file = FileOperations::openFile(readFilenameSetting.getString(), "rb");
	if (!file) {
		throw PlugException("Failed to open input: ", strerror(errno));
	}

	auto& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread = std::thread([this]() { run(); });
}

void MidiInReader::unplugHelper(EmuTime::param /*time*/)
{
	poller.abort();
	thread.join();
	file.reset();
}

const string& MidiInReader::getName() const
{
	static const string name("midi-in-reader");
	return name;
}

std::string_view MidiInReader::getDescription() const
{
	return "MIDI in file reader. Sends data from an input file to the "
	       "MIDI port it is connected to. The filename is set with "
	       "the 'midi-in-readfilename' setting.";
}


void MidiInReader::run()
{
	if (!file) return;
	while (true) {
#ifndef _WIN32
		if (poller.poll(fileno(file.get()))) {
			break;
		}
#endif
		byte buf;
		size_t num = fread(&buf, 1, 1, file.get());
		if (poller.aborted()) {
			break;
		}
		if (num != 1) {
			continue;
		}
		assert(isPluggedIn());

		{
			std::lock_guard<std::mutex> lock(mutex);
			queue.push_back(buf);
		}
		eventDistributor.distributeEvent(
			std::make_shared<SimpleEvent>(OPENMSX_MIDI_IN_READER_EVENT));
	}
}

// MidiInDevice
void MidiInReader::signal(EmuTime::param time)
{
	auto* conn = static_cast<MidiInConnector*>(getConnector());
	if (!conn->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}
	if (!conn->ready()) {
		return;
	}

	byte data;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	conn->recvByte(data, time);
}

// EventListener
int MidiInReader::signalEvent(const std::shared_ptr<const Event>& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
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
