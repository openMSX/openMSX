// $Id$

#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Thread.hh"
#include "SettingListener.hh"
#include "Socket.hh"
#include <memory>

namespace openmsx {

class Scheduler;
class CommandController;
class CliComm;
class IntegerSetting;
template <typename> class EnumSetting;

class CliServer : private Runnable, private SettingListener
{
public:
	CliServer(Scheduler& scheduler, CommandController& commandController);
	~CliServer();

private:
	// Runnable
	virtual void run();

	// SettingListener
	virtual void update(const Setting* setting);

	void start();
	void stop();
	void mainLoop();
	int openPort(SOCKET listenSock);
	Thread thread;
	SOCKET listenSock;

	Scheduler& scheduler;
	CommandController& commandController;
	CliComm& cliComm;

	enum ListenHost { NONE, LOCAL, ALL };
	std::auto_ptr<IntegerSetting> listenPortMin;
	std::auto_ptr<IntegerSetting> listenPortMax;
	std::auto_ptr<EnumSetting<ListenHost> > listenHost;
};

} // namespace openmsx

#endif
