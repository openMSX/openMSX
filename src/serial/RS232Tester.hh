// $Id$

#ifndef RS232TESTER_HH
#define RS232TESTER_HH

#include "openmsx.hh"
#include "RS232Device.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include <fstream>
#include <cstdio>
#include <deque>
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;

class RS232Tester : public RS232Device, private Runnable, private Schedulable
{
public:
	RS232Tester(Scheduler& scheduler, CommandController& commandController);
	virtual ~RS232Tester();

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// input
	virtual void signal(const EmuTime& time);

	// output
	virtual void recvByte(byte value, const EmuTime& time);

private:
	// Runnable
	virtual void run();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	Thread thread;
	FILE* inFile;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue

	std::ofstream outFile;

	const std::auto_ptr<FilenameSetting> rs232InputFilenameSetting;
	const std::auto_ptr<FilenameSetting> rs232OutputFilenameSetting;
};

} // namespace openmsx

#endif
