#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
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
	static volatile bool exitLoop;

	// Runnable
	void run() override;

	void mainLoop();
	SOCKET createSocket();
	bool exitAcceptLoop();

	CommandController& commandController;
	EventDistributor& eventDistributor;
	GlobalCliComm& cliComm;

	Thread thread;
	std::string socketName;
	SOCKET listenSock;
};

} // namespace openmsx

#endif
