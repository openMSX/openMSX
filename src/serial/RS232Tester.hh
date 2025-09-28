#ifndef RS232TESTER_HH
#define RS232TESTER_HH

#include "RS232Device.hh"

#include "EventListener.hh"
#include "FileOperations.hh"
#include "FilenameSetting.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class RS232Tester final : public RS232Device, private EventListener
{
public:
	RS232Tester(EventDistributor& eventDistributor, Scheduler& scheduler,
	            CommandController& commandController);
	~RS232Tester() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::optional<bool> getDSR(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getCTS(EmuTime time) const override;

	// input
	void signal(EmuTime time) override;

	// output
	void recvByte(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	bool signalEvent(const Event& event) override;

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	FileOperations::FILE_t inFile;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue
	std::optional<Poller> poller;

	std::ofstream outFile;

	FilenameSetting rs232InputFilenameSetting;
	FilenameSetting rs232OutputFilenameSetting;
};

} // namespace openmsx

#endif
