#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Poller.hh"
#include "Socket.hh"
#include "GlobalSettings.hh"
#include <string>
#include <thread>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalCliComm;
class GlobalSettings;

class CliServer final
{
public:
	CliServer(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          GlobalCliComm& cliCom,
	          GlobalSettings& globalSettings);
	~CliServer();

private:
	void mainLoop();
	[[nodiscard]] SOCKET createSocket();
	[[nodiscard]] int openPort(SOCKET listenSock);
	[[nodiscard]] void setCurrentSocketPortNumber(int port) { globalSettings.getSocketSettingsManager().setCurrentSocketPortNumber(port); }
	void exitAcceptLoop();

private:
	CommandController& commandController;
	EventDistributor& eventDistributor;
	GlobalCliComm& cliComm;
	GlobalSettings& globalSettings;

	std::thread thread;
	std::string socketName;
	SOCKET listenSock;
	Poller poller;
};

} // namespace openmsx

#endif
