#include "RS232Tester.hh"
#include "RS232Connector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
#include "serialize.hh"

namespace openmsx {

RS232Tester::RS232Tester(EventDistributor& eventDistributor_,
                         Scheduler& scheduler_,
                         CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, rs232InputFilenameSetting(
	        commandController, "rs232-inputfilename",
	        "filename of the file where the RS232 input is read from",
	        "rs232-input")
	, rs232OutputFilenameSetting(
	        commandController, "rs232-outputfilename",
	        "filename of the file where the RS232 output is written to",
	        "rs232-output")
{
	eventDistributor.registerEventListener(EventType::RS232_TESTER, *this);
}

RS232Tester::~RS232Tester()
{
	eventDistributor.unregisterEventListener(EventType::RS232_TESTER, *this);
}

// Pluggable
void RS232Tester::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	// output
	auto outName = rs232OutputFilenameSetting.getString();
	FileOperations::openofstream(outFile, outName);
	if (outFile.fail()) {
		outFile.clear();
		throw PlugException("Error opening output file: ", outName);
	}

	// input
	auto inName = rs232InputFilenameSetting.getString();
	inFile = FileOperations::openFile(inName, "rb");
	if (!inFile) {
		outFile.close();
		throw PlugException("Error opening input file: ", inName);
	}

	auto& rs232Connector = static_cast<RS232Connector&>(connector_);
	rs232Connector.setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	rs232Connector.setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	rs232Connector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread = std::thread([this]() { run(); });
}

void RS232Tester::unplugHelper(EmuTime::param /*time*/)
{
	// output
	outFile.close();

	// input
	poller.abort();
	thread.join();
	inFile.reset();
}

std::string_view RS232Tester::getName() const
{
	return "rs232-tester";
}

std::string_view RS232Tester::getDescription() const
{
	return	"RS232 tester pluggable. Reads all data from file specified "
		"with the 'rs-232-inputfilename' setting. Writes all data "
		"to the file specified with the 'rs232-outputfilename' "
		"setting.";
}

void RS232Tester::run()
{
	if (!inFile) return;
	while (!feof(inFile.get())) {
#ifndef _WIN32
		if (poller.poll(fileno(inFile.get()))) {
			break;
		}
#endif
		byte buf[1];
		size_t num = fread(buf, 1, 1, inFile.get());
		if (poller.aborted()) {
			break;
		}
		if (num != 1) {
			continue;
		}
		assert(isPluggedIn());
		std::lock_guard<std::mutex> lock(mutex);
		queue.push_back(buf[0]);
		eventDistributor.distributeEvent(
			Event::create<Rs232TesterEvent>());
	}
}

// input
void RS232Tester::signal(EmuTime::param time)
{
	auto* conn = static_cast<RS232Connector*>(getConnector());
	if (!conn->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}
	if (!conn->ready()) return;

	std::lock_guard<std::mutex> lock(mutex);
	if (queue.empty()) return;
	conn->recvByte(queue.pop_front(), time);
}

// EventListener
int RS232Tester::signalEvent(const Event& /*event*/) noexcept
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
	}
	return 0;
}


// output
void RS232Tester::recvByte(byte value, EmuTime::param /*time*/)
{
	if (outFile.is_open()) {
		outFile.put(value);
		outFile.flush();
	}
}


template<typename Archive>
void RS232Tester::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(RS232Tester);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Tester, "RS232Tester");

} // namespace openmsx
