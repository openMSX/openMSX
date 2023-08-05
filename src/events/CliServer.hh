#ifndef CLISERVER_HH
#define CLISERVER_HH

#include "Poller.hh"
#include "Socket.hh"
#include "GlobalSettings.hh"

#ifdef _WIN32
#include "Observer.hh"
#include "SocketSettingsManager.hh"
#endif

#include <string>
#include <thread>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalCliComm;
class GlobalSettings;

#ifdef _WIN32
class SocketSettingsManager;
#endif

class CliServer final 
#ifdef _WIN32
	: private Observer<SocketSettingsManager>
#endif
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
	void deleteSocketFile(const std::string& socket);

#ifdef _WIN32
	// Observer<SpeedManager>
	void update(const SocketSettingsManager& socketSettingsManager) noexcept override;
#endif

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
