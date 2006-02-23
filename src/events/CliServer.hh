// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Socket.hh"
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;
class CliComm;

class CliServer : private Runnable
{
public:
	CliServer(CommandController& commandController,
	          EventDistributor& eventDistributor);
	~CliServer();

private:
	// Runnable
	virtual void run();

	void mainLoop();
	void createSocket();
	Thread thread;
	std::string socketName;
	SOCKET listenSock;

	CommandController& commandController;
	EventDistributor& eventDistributor;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
