// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"

namespace openmsx {

class CliServer : private Runnable
{
public:
	CliServer();
	~CliServer();

private:
	virtual void run();
	void mainLoop();
	Thread thread;
};

} // namespace openmsx

#endif
