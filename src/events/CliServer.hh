// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Socket.hh"
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalCliComm;

class CliServer : private Runnable
{
public:
	CliServer(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          GlobalCliComm& cliComm);
	~CliServer();

private:
	// Runnable
	virtual void run();

	void mainLoop();
	void createSocket();
	bool exitAcceptLoop();

	CommandController& commandController;
	EventDistributor& eventDistributor;
	GlobalCliComm& cliComm;

	Thread thread;
	std::string socketName;
	SOCKET listenSock;
	bool exitLoop;

};

} // namespace openmsx

#endif
