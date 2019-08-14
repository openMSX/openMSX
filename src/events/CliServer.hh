#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Poller.hh"
#include "Socket.hh"
#include <string>
#include <thread>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalCliComm;

class CliServer final
{
public:
	CliServer(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          GlobalCliComm& cliComm);
	~CliServer();

private:
	void mainLoop();
	SOCKET createSocket();
	void exitAcceptLoop();

	CommandController& commandController;
	EventDistributor& eventDistributor;
	GlobalCliComm& cliComm;

	std::thread thread;
	std::string socketName;
	SOCKET listenSock;
	Poller poller;
};

} // namespace openmsx

#endif
