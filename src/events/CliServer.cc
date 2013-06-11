#include "CliServer.hh"
#include "GlobalCliComm.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "memory.hh"
#include "statp.hh"
#include <string>

#ifdef _WIN32
#include <fstream>
#include <cstdlib>
#include <ctime>
#else
#include <pwd.h>
#endif

using std::string;


namespace openmsx {

static string getUserName()
{
#if defined(_WIN32)
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
	string_ref name = FileOperations::getFilename(socket);
	if (!name.starts_with("socket.")) {
		return false; // wrong name
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

	srand(unsigned(time(nullptr))); // easily predicatble, but doesn't matter
	int first = rand();

	for (int n = 0; n < RANGE; ++n) {
		int port = BASE + ((first + n) % RANGE);
		sockaddr_in server_address;
		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_address.sin_port = htons(port);
		if (bind(listenSock, reinterpret_cast<sockaddr*>(&server_address),
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
	socketName = StringOp::Builder() << dir << "/socket." << int(getpid());

#ifdef _WIN32
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}
	int portNumber = openPort(listenSock);

	// write port number to file
	FileOperations::unlink(socketName); // ignore error
	std::ofstream out;
	FileOperations::openofstream(out, socketName);
	out << portNumber << std::endl;
	if (!out.good()) {
		throw MSXException("Couldn't open socket.");
	}

#else
	listenSock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listenSock == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}

	FileOperations::unlink(socketName); // ignore error

	sockaddr_un addr;
	strcpy(addr.sun_path, socketName.c_str());
	addr.sun_family = AF_UNIX;
	if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
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

// The BSD socket API does not contain a simple way to cancel a call to
// accept(). As a workaround, we connect to the server socket ourselves.
bool CliServer::exitAcceptLoop()
{
#ifdef _WIN32
	// Windows application-level firewalls pop up a warning about openMSX
	// connecting to itself. Since closing the socket is sufficient to make
	// Windows exit the accept() call, this code is disabled on Windows.
	return true;
	/*
	SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr;
	memset((char*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(portNumber);
	*/
#else
	SOCKET sd = socket(AF_UNIX, SOCK_STREAM, 0);
	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socketName.c_str());
// Code below is OS-independent, but unreachable on Windows:
	if (sd == OPENMSX_INVALID_SOCKET) {
		return false;
	}
	int r = connect(sd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	close(sd);
	return r != SOCKET_ERROR;
#endif
}

static void deleteSocket(const string& socket)
{
	FileOperations::unlink(socket); // ignore errors
	string dir = socket.substr(0, socket.find_last_of('/'));
	FileOperations::rmdir(dir); // ignore errors
}


CliServer::CliServer(CommandController& commandController_,
                     EventDistributor& eventDistributor_,
                     GlobalCliComm& cliComm_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, cliComm(cliComm_)
	, thread(this)
	, listenSock(OPENMSX_INVALID_SOCKET)
{
	exitLoop = false;
	sock_startup();
	try {
		createSocket();
		thread.start();
	} catch (MSXException& e) {
		cliComm.printWarning(e.getMessage());
	}
}

CliServer::~CliServer()
{
	exitLoop = true;
	if (listenSock != OPENMSX_INVALID_SOCKET) {
		sock_close(listenSock);
		if (!exitAcceptLoop()) {
			// clean exit failed, try emergency exit
			thread.stop();
		}
	}
	thread.join();

	deleteSocket(socketName);
	sock_cleanup();
}

void CliServer::run()
{
	mainLoop();
}

void CliServer::mainLoop()
{
	while (!exitLoop) {
		// wait for incomming connection
		SOCKET sd = accept(listenSock, nullptr, nullptr);
		if (sd == OPENMSX_INVALID_SOCKET) {
			// sock_close(listenSock);  // hangs on win32
			return;
		}
		CliListener* connection = new SocketConnection(
				commandController, eventDistributor, sd);
		cliComm.addListener(connection);
	}
}

} // namespace openmsx
