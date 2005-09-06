// $Id$

#include "CliServer.hh"
#include "CommandController.hh"
#include "IntegerSetting.hh"
#include "EnumSetting.hh"
#include "CliComm.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include <cassert>

namespace openmsx {

CliServer::CliServer(Scheduler& scheduler_,
                     CommandController& commandController_)
	: thread(this)
	, scheduler(scheduler_)
	, commandController(commandController_)
	, cliComm(commandController.getCliComm())
{
	listenPortMin.reset(new IntegerSetting(commandController,
		"listen_port_min",
		"Use together with list_port_max. These indicate a "
		"range of TCP/IP port numbers to listen on for "
		"incomming control connections. OpenMSX will take the "
		"lowest available port. (There is a range of ports to "
		"allow multiple simultaneous openMSX sessions.)",
		9938, 0, 65536));
	listenPortMax.reset(new IntegerSetting(commandController,
		"listen_port_max",
		"See listen_min_port.", 9958, 0, 65536));

	EnumSetting<ListenHost>::Map hostMap;
	hostMap["none"]  = NONE;
	hostMap["local"] = LOCAL;
	hostMap["all"]   = ALL;
	listenHost.reset(new EnumSetting<ListenHost>(commandController,
		"listen_host",
		"Only allow incomming connections from the specified "
		"host(s). Possible values are 'any', 'local' or 'none'. "
		"We strongly advice against using 'all' on non-trusted "
		"networks (such as the internet).",
		LOCAL, hostMap));

	sock_startup();
	start();

	listenPortMin->addListener(this);
	listenPortMax->addListener(this);
	listenHost   ->addListener(this);
}

CliServer::~CliServer()
{
	listenHost   ->removeListener(this);
	listenPortMax->removeListener(this);
	listenPortMin->removeListener(this);

	stop();
	sock_cleanup();
}

void CliServer::run()
{
	try {
		mainLoop();
	} catch (MSXException& e) {
		cliComm.printWarning(e.getMessage());
	}
}

void CliServer::start()
{
	thread.start();
}

void CliServer::stop()
{
	sock_close(listenSock);
	thread.stop();
}

int CliServer::openPort(SOCKET listenSock)
{
	unsigned addr; // win32 doesn't have in_addr_t
	switch (listenHost->getValue()) {
		case NONE:
			return -1;
		case LOCAL:
			addr = htonl(INADDR_LOOPBACK);
			break;
		case ALL:
			addr = htonl(INADDR_ANY);
			break;
		default:
			assert(false);
			return -1;
	}

	int min = listenPortMin->getValue();
	int max = listenPortMax->getValue();
	for (int port = min; port < max; ++port) {
		sockaddr_in server_address;
		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = addr;
		server_address.sin_port = htons(port);
		if (bind(listenSock, (sockaddr*)&server_address,
		         sizeof(server_address)) != SOCKET_ERROR) {
			return port;
		}
	}
	throw MSXException("Couldn't open socket.");
}

void CliServer::mainLoop()
{
	// setup listening socket
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		throw MSXException(sock_error());
	}
	sock_reuseAddr(listenSock);

	int port = openPort(listenSock);
	if (port == -1) {
		sock_close(listenSock);
		return;
	}
	cliComm.printInfo("Listening on port " + StringOp::toString(port) +
	                  " for incoming connections.");
	listen(listenSock, SOMAXCONN);

	while (true) {
		// wait for incomming connection
		SOCKET sd = accept(listenSock, NULL, NULL);
		if (sd == INVALID_SOCKET) {
			// sock_close(listenSock);  // hangs on win32
			return;
		}
		cliComm.connections.push_back(new SocketConnection(
				scheduler, commandController, sd));
	}
}

void CliServer::update(const Setting* /*setting*/)
{
	// restart listener thread
	stop();
	start();
}

} // namespace openmsx
