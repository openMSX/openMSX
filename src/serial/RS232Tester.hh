#ifndef RS232TESTER_HH
#define RS232TESTER_HH

#include "RS232Device.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "FileOperations.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include "circular_buffer.hh"
#include <fstream>
#include <cstdio>
#include <memory>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;
class FilenameSetting;

class RS232Tester final : public RS232Device, private Runnable
                        , private EventListener
{
public:
	RS232Tester(EventDistributor& eventDistributor, Scheduler& scheduler,
	            CommandController& commandController);
	~RS232Tester();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	const std::string& getName() const override;
	string_ref getDescription() const override;

	// input
	void signal(EmuTime::param time) override;

	// output
	void recvByte(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Runnable
	void run() override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	Thread thread;
	FileOperations::FILE_t inFile;
	cb_queue<byte> queue;
	Semaphore lock; // to protect queue

	std::ofstream outFile;

	const std::unique_ptr<FilenameSetting> rs232InputFilenameSetting;
	const std::unique_ptr<FilenameSetting> rs232OutputFilenameSetting;
};

} // namespace openmsx

#endif
