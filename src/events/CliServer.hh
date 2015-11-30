#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Socket.hh"
#include <atomic>
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalCliComm;

class CliServer final : private Runnable
{
public:
	CliServer(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          GlobalCliComm& cliComm);
	~CliServer();

private:
	// Runnable
	void run() override;

	void mainLoop();
	SOCKET createSocket();
	void exitAcceptLoop();

	CommandController& commandController;
	EventDistributor& eventDistributor;
	GlobalCliComm& cliComm;

	Thread thread;
	std::string socketName;
	SOCKET listenSock;
#ifndef _WIN32
	int wakeupPipe[2];
#endif
	std::atomic_bool exitLoop;
};

} // namespace openmsx

#endif
