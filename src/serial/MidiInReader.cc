#include "MidiInReader.hh"
#include "MidiInConnector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <array>
#include <cstdio>
#include <cerrno>
#include <cstring>

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
	eventDistributor.registerEventListener(EventType::MIDI_IN_READER, *this);
}

MidiInReader::~MidiInReader()
{
	eventDistributor.unregisterEventListener(EventType::MIDI_IN_READER, *this);
}

// Pluggable
void MidiInReader::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	file = FileOperations::openFile(readFilenameSetting.getString(), "rb");
	if (!file) {
		throw PlugException("Failed to open input: ", strerror(errno));
	}

	auto& midiConnector = checked_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DataBits::D8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::StopBits::S1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::Parity::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	poller.emplace();
	thread = std::thread([this]() { run(); });
}

void MidiInReader::unplugHelper(EmuTime::param /*time*/)
{
	poller->abort();
	thread.join();
	poller.reset();
	file.reset();
}

std::string_view MidiInReader::getName() const
{
	return "midi-in-reader";
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
		if (poller->poll(fileno(file.get()))) {
			break;
		}
#endif
		std::array<uint8_t, 1> buf;
		size_t num = fread(buf.data(), sizeof(uint8_t), buf.size(), file.get());
		if (poller->aborted()) {
			break;
		}
		if (num != 1) {
			continue;
		}
		assert(isPluggedIn());

		{
			std::scoped_lock lock(mutex);
			queue.push_back(buf[0]);
		}
		eventDistributor.distributeEvent(MidiInReaderEvent());
	}
}

// MidiInDevice
void MidiInReader::signal(EmuTime::param time)
{
	auto* conn = checked_cast<MidiInConnector*>(getConnector());
	if (!conn->acceptsData()) {
		std::scoped_lock lock(mutex);
		queue.clear();
		return;
	}
	if (!conn->ready()) {
		return;
	}

	uint8_t data;
	{
		std::scoped_lock lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	conn->recvByte(data, time);
}

// EventListener
bool MidiInReader::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return false;
}


template<typename Archive>
void MidiInReader::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(MidiInReader);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInReader, "MidiInReader");

} // namespace openmsx
