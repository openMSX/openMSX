// $Id$

#include "RS232Tester.hh"
#include "RS232Connector.hh"
#include "Scheduler.hh"

using std::string;

namespace openmsx {

RS232Tester::RS232Tester()
	: thread(this), lock(1),
	  rs232InputFilenameSetting("rs232-inputfilename",
	        "filename of the file where the RS232 input is read from",
	        "rs232-input"),
	  rs232OutputFilenameSetting("rs232-outputfilename",
	        "filename of the file where the RS232 output is written to",
	        "rs232-output")
{
}

RS232Tester::~RS232Tester()
{
	Scheduler::instance().removeSyncPoint(this);
}

// Pluggable
void RS232Tester::plugHelper(Connector* connector_, const EmuTime& /*time*/)
{
	// output
	outFile.open(rs232OutputFilenameSetting.getValue().c_str());
	if (outFile.fail()) {
		outFile.clear();
		throw PlugException("Error opening output file");
	}

	// input
	inFile = fopen(rs232InputFilenameSetting.getValue().c_str(), "rb");
	if (!inFile) {
		throw PlugException("Error opening input file");
	}

	RS232Connector* rs232Connector = static_cast<RS232Connector*>(connector_);
	rs232Connector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	rs232Connector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	rs232Connector->setParityBit(false, SerialDataInterface::EVEN); // no parity

	connector = connector_; // base class will do this in a moment,
	                        // but thread already needs it
	thread.start();
}

void RS232Tester::unplugHelper(const EmuTime& /*time*/)
{
	// output
	outFile.close();

	// input
	lock.down();
	thread.stop();
	lock.up();
	fclose(inFile);
}

const string& RS232Tester::getName() const
{
	static const string name("rs232-tester");
	return name;
}

const string& RS232Tester::getDescription() const
{
	static const string desc(
		"RS232 tester pluggable. Reads all data from file specified "
		"with the 'rs-232-inputfilename' setting. Writes all data "
		"to the file specified with the 'rs232-outputfilename' "
		"setting.");
	return desc;
}

// Runnable
void RS232Tester::run()
{
	byte buf;
	while (true) {
		int num = fread(&buf, 1, 1, inFile);
		if (num != 1) {
			continue;
		}
		assert(getConnector());
		lock.down();
		queue.push_back(buf);
		Scheduler::instance().setSyncPoint(Scheduler::ASAP, this);
		lock.up();
	}
}

// input
void RS232Tester::signal(const EmuTime& time)
{
	RS232Connector* connector = static_cast<RS232Connector*>(getConnector());
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
void RS232Tester::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (getConnector()) {
		signal(time);
	} else {
		lock.down();
		queue.empty();
		lock.up();
	}
}

const string& RS232Tester::schedName() const
{
	return getName();
}


// output
void RS232Tester::recvByte(byte value, const EmuTime& /*time*/)
{
	outFile.put(value);
	outFile.flush();
}

} // namespace openmsx
