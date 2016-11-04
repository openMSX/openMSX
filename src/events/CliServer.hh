#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Poller.hh"
#include "Socket.hh"
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
	Poller poller;
};

} // namespace openmsx

#endif
