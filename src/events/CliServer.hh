// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Socket.hh"
#include <string>

namespace openmsx {

class Scheduler;
class CommandController;
class CliComm;

class CliServer : private Runnable
{
public:
	CliServer(Scheduler& scheduler, CommandController& commandController);
	~CliServer();

private:
	// Runnable
	virtual void run();

	void mainLoop();
	void createSocket();
	Thread thread;
	std::string socketName;
	SOCKET listenSock;

	Scheduler& scheduler;
	CommandController& commandController;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
