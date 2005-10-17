// $Id$

#include "CliServer.hh"
#include "CommandController.hh"
#include "IntegerSetting.hh"
#include "EnumSetting.hh"
#include "CliComm.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include <string>
#include <cassert>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <fstream>
#include <cstdlib>
#include <time.h>
#else
#include <pwd.h>
#endif

using std::string;


namespace openmsx {

static string getUserName()
{
#ifdef _WIN32
	return "default";
#else
	struct passwd* pw = getpwuid(getuid());
	return pw->pw_name ? pw->pw_name : "";
#endif
}

static bool checkSocketDir(const string& dir)
{
	struct stat st;
	if (stat(dir.c_str(), &st)) {
		// cannot stat
		return false;
	}
	if (!S_ISDIR(st.st_mode)) {
		// not a directory
		return false;
	}
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st.st_mode & 0777) != 0700) {
		// wrong permissions
		return false;
	}
	if (st.st_uid != getuid()) {
		// wrong uid
		return false;
	}
#endif
	return true;
}
  
static bool checkSocket(const string& socket)
{
	string dir  = socket.substr(0, socket.find_last_of('/'));
	string name = socket.substr(socket.find_last_of('/') + 1);

	if (name.substr(0, 7) != "socket.") {
		// wrong name
		return false;
	}

	struct stat st;
	if (stat(socket.c_str(), &st)) {
		// cannot stat
		return false;
	}
#ifdef _WIN32
	if (!S_ISREG(st.st_mode)) {
		// not a regular file
		return false;
	}
#else
	if (!S_ISSOCK(st.st_mode)) {
		// not a socket
		return false;
	}
#endif
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st.st_mode & 0777) != 0600) {
		// check will be different on win32 (!= 777) thus actually useless 
		// wrong permissions
		return false;
	}
	if (st.st_uid != getuid()) {
		// does this work on win32? is this check meaningful?
		// wrong uid
		return false;
	}
#endif
	return true;
}

#ifdef _WIN32
static int openPort(SOCKET listenSock)
{
	const int BASE = 9938;
	const int RANGE = 64;

	srand(time(0)); // easily predicatble, but doesn't matter
	int first = rand();

	for (int n = 0; n < RANGE; ++n) {
		int port = BASE + ((first + n) % RANGE);
		sockaddr_in server_address;
		memset((char*)&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_address.sin_port = htons(port);
		if (bind(listenSock, (sockaddr*)&server_address,
					sizeof(server_address)) != -1) {
			return port;
		}
	}
	throw MSXException("Couldn't open socket.");
}
#endif

void CliServer::createSocket()
{
	string dir = FileOperations::getTempDir() + "/openmsx-" + getUserName();
	FileOperations::mkdir(dir, 0700);
	if (!checkSocketDir(dir)) {
		throw MSXException("Couldn't create socket directory.");
	}
	socketName = dir + "/socket." + StringOp::toString(getpid());  

#ifdef _WIN32
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		throw MSXException(sock_error());
	}
	int port = openPort(listenSock);

	// write port number to file
	unlink(socketName.c_str()); // ignore error
	std::ofstream out(socketName.c_str());
	out << port << std::endl;
	if (!out.good()) {
		throw MSXException("Couldn't open socket.");
	}

#else
	listenSock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		throw MSXException(sock_error());
	}

	unlink(socketName.c_str()); // ignore error

	sockaddr_un addr; 
	strcpy(addr.sun_path, socketName.c_str()); 
	addr.sun_family = AF_UNIX; 
	if (bind(listenSock, (sockaddr*)&addr, sizeof(sockaddr_un)) == -1) {
		throw MSXException("Couldn't open socket.");
	}
	if (chmod(socketName.c_str(), 0600) == -1) {
		throw MSXException("Couldn't open socket.");
	}

#endif
	if (!checkSocket(socketName)) {
		throw MSXException("Couldn't open socket.");
	}
	listen(listenSock, SOMAXCONN);
}

static void deleteSocket(const string& socket)
{
	unlink(socket.c_str()); // ignore errors
	string dir = socket.substr(0, socket.find_last_of('/'));
	rmdir(dir.c_str()); // ignore errors
}


CliServer::CliServer(Scheduler& scheduler_,
                     CommandController& commandController_)
	: thread(this)
	, listenSock(INVALID_SOCKET)
	, scheduler(scheduler_)
	, commandController(commandController_)
	, cliComm(commandController.getCliComm())
{
	sock_startup();
	thread.start();
}

CliServer::~CliServer()
{
	if (listenSock != INVALID_SOCKET) {
		sock_close(listenSock);
	}
	deleteSocket(socketName);

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

void CliServer::mainLoop()
{
	createSocket();

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

} // namespace openmsx
