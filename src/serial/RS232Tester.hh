// $Id$

#ifndef RS232TESTER_HH
#define RS232TESTER_HH

#include "RS232Device.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <fstream>
#include <cstdio>
#include <deque>
#include <memory>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;
class FilenameSetting;

class RS232Tester : public RS232Device, private Runnable, private EventListener
{
public:
	RS232Tester(EventDistributor& eventDistributor, Scheduler& scheduler,
	            CommandController& commandController);
	virtual ~RS232Tester();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;

	// input
	virtual void signal(EmuTime::param time);

	// output
	virtual void recvByte(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	virtual void run();

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	FILE* inFile;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue

	std::ofstream outFile;

	const std::unique_ptr<FilenameSetting> rs232InputFilenameSetting;
	const std::unique_ptr<FilenameSetting> rs232OutputFilenameSetting;
};

} // namespace openmsx

#endif
