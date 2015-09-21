#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <atomic>
#include <cstdio>
#include <cerrno>
#include <cstring>

using std::string;

namespace openmsx {

// This only works for one simultaneous instance
// TODO get rid of helper thread
static std::atomic<bool> exitLoop(false);

MidiInReader::MidiInReader(EventDistributor& eventDistributor_,
                           Scheduler& scheduler_,
                           CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, thread(this)
	, readFilenameSetting(
		commandController, "midi-in-readfilename",
		"filename of the file where the MIDI input is read from",
		"/dev/midi")
{
	eventDistributor.registerEventListener(OPENMSX_MIDI_IN_READER_EVENT, *this);
	exitLoop = false;
}

MidiInReader::~MidiInReader()
{
	exitLoop = true;
	eventDistributor.unregisterEventListener(OPENMSX_MIDI_IN_READER_EVENT, *this);
}

// Pluggable
void MidiInReader::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	file = FileOperations::openFile(readFilenameSetting.getString().str(), "rb");
	if (!file) {
		throw PlugException(StringOp::Builder()
			<< "Failed to open input: " << strerror(errno));
	}

	auto& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread.start();
}

void MidiInReader::unplugHelper(EmuTime::param /*time*/)
{
	std::lock_guard<std::mutex> lock(mutex);
	thread.stop();
	file.reset();
}

const string& MidiInReader::getName() const
{
	static const string name("midi-in-reader");
	return name;
}

string_ref MidiInReader::getDescription() const
{
	return "MIDI in file reader. Sends data from an input file to the "
	       "MIDI port it is connected to. The filename is set with "
	       "the 'midi-in-readfilename' setting.";
}


// Runnable
void MidiInReader::run()
{
	byte buf;
	if (!file) return;
	while (true) {
		size_t num = fread(&buf, 1, 1, file.get());
		if (exitLoop) break;
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
