// $Id$

#include "RS232Tester.hh"
#include "RS232Connector.hh"
#include "Scheduler.hh"


namespace openmsx {

RS232Tester::RS232Tester()
	: thread(this), connector(NULL), lock(1),
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
	Scheduler::instance()->removeSyncPoint(this);
}

// Pluggable
void RS232Tester::plug(Connector *connector_, const EmuTime &time)
	throw(PlugException)
{
	// output
	outFile.open(rs232OutputFilenameSetting.getValue().c_str());
	if (outFile.fail()) {
		throw PlugException("Error opening output file");
	}

	// input
	inFile = fopen(rs232InputFilenameSetting.getValue().c_str(), "rb");
	if (!inFile) {
		throw PlugException("Error opening input file");
	}

	connector = (RS232Connector *)connector_;
	connector->setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	connector->setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	connector->setParityBit(false, SerialDataInterface::EVEN); // no parity

	thread.start();
}

void RS232Tester::unplug(const EmuTime &time)
{
	// output
	outFile.close();

	// input
	lock.down();
	thread.stop();
	lock.up();
	connector = NULL;
	fclose(inFile);
}

const string& RS232Tester::getName() const
{
	static const string name("rs232-tester");
	return name;
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
		assert(connector);
		lock.down();
		queue.push_back(buf);
		Scheduler::instance()->setSyncPoint(Scheduler::ASAP, this);
		lock.up();
	}
}

// input
void RS232Tester::signal(const EmuTime &time)
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
void RS232Tester::executeUntilEmuTime(const EmuTime &time, int userData)
{
	if (connector) {
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
void RS232Tester::recvByte(byte value, const EmuTime &time)
{
	outFile.put(value);
	outFile.flush();
}

} // namespace openmsx
