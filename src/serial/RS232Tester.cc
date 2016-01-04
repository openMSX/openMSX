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
	, thread(this)
	, rs232InputFilenameSetting(
	        commandController, "rs232-inputfilename",
	        "filename of the file where the RS232 input is read from",
	        "rs232-input")
	, rs232OutputFilenameSetting(
	        commandController, "rs232-outputfilename",
	        "filename of the file where the RS232 output is written to",
	        "rs232-output")
{
	eventDistributor.registerEventListener(OPENMSX_RS232_TESTER_EVENT, *this);
}

RS232Tester::~RS232Tester()
{
	eventDistributor.unregisterEventListener(OPENMSX_RS232_TESTER_EVENT, *this);
}

// Pluggable
void RS232Tester::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	// output
	string_ref outName = rs232OutputFilenameSetting.getString();
	FileOperations::openofstream(outFile, outName.str());
	if (outFile.fail()) {
		outFile.clear();
		throw PlugException("Error opening output file: " + outName);
	}

	// input
	string_ref inName = rs232InputFilenameSetting.getString();
	inFile = FileOperations::openFile(inName.str(), "rb");
	if (!inFile) {
		outFile.close();
		throw PlugException("Error opening input file: " + inName);
	}

	auto& rs232Connector = static_cast<RS232Connector&>(connector_);
	rs232Connector.setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	rs232Connector.setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	rs232Connector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread.start();
}

void RS232Tester::unplugHelper(EmuTime::param /*time*/)
{
	// output
	outFile.close();

	// input
	std::lock_guard<std::mutex> lock(mutex);
	thread.stop();
	inFile.reset();
}

const std::string& RS232Tester::getName() const
{
	static const std::string name("rs232-tester");
	return name;
}

string_ref RS232Tester::getDescription() const
{
	return	"RS232 tester pluggable. Reads all data from file specified "
		"with the 'rs-232-inputfilename' setting. Writes all data "
		"to the file specified with the 'rs232-outputfilename' "
		"setting.";
}

// Runnable
void RS232Tester::run()
{
	byte buf;
	if (!inFile) return;
	while (!feof(inFile.get())) {
		size_t num = fread(&buf, 1, 1, inFile.get());
		if (num != 1) {
			continue;
		}
		assert(isPluggedIn());
		std::lock_guard<std::mutex> lock(mutex);
		queue.push_back(buf);
		eventDistributor.distributeEvent(
			std::make_shared<SimpleEvent>(OPENMSX_RS232_TESTER_EVENT));
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
int RS232Tester::signalEvent(const std::shared_ptr<const Event>& /*event*/)
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
