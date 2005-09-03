// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "Socket.hh"

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
	virtual void run();
	void mainLoop();
	Thread thread;
	SOCKET listenSock;

	Scheduler& scheduler;
	CommandController& commandController;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
