// $Id$

#include "CliServer.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include "MSXException.hh"

namespace openmsx {

CliServer::CliServer(Scheduler& scheduler_,
                     CommandController& commandController_)
	: thread(this)
	, scheduler(scheduler_)
	, commandController(commandController_)
	, cliComm(commandController.getCliComm())
{
	sock_startup();
	thread.start();
}

CliServer::~CliServer()
{
	sock_close(listenSock);
	thread.stop();
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

static int openPort(SOCKET listenSock, int min, int max)
{
	for (int port = min; port < max; ++port) {
		sockaddr_in server_address;
		memset((char*)&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_address.sin_port = htons(port);
		if (bind(listenSock, (sockaddr*)&server_address,
		         sizeof(server_address))
		    != SOCKET_ERROR) {
			return port;
		}
	}
	return -1;
}

void CliServer::mainLoop()
{
	// setup listening socket
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		throw FatalError(sock_error());
	}
	sock_reuseAddr(listenSock);

	int port = openPort(listenSock, 9938, 9958);
	if (port == -1) {
		sock_close(listenSock);
		throw FatalError("Couldn't open socket.");
		return;
	}
	cliComm.printInfo(
		"Listening on port " + StringOp::toString(port) +
		" for incoming (local) connections.");
	listen(listenSock, SOMAXCONN);

	// main loop
	while (true) {
		// We have a new connection coming in!
		SOCKET sd = accept(listenSock, NULL, NULL);
		if (sd == INVALID_SOCKET) {
			sock_close(listenSock);
			// TODO throw here or not?
			// throw FatalError(sock_error());
			return;
		}
		cliComm.connections.push_back(new SocketConnection(
				scheduler, commandController, sd));
	}
}

} // namespace openmsx
