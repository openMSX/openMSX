#include "CliServer.hh"
#include "GlobalCliComm.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "memory.hh"
#include "random.hh"
#include "statp.hh"
#include <string>

#ifdef _WIN32
#include <fstream>
#include <ctime>
#else
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
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

	int first = random_int(0, RANGE - 1); // [0, RANGE)

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

SOCKET CliServer::createSocket()
{
	string dir = FileOperations::getTempDir() + "/openmsx-" + getUserName();
	FileOperations::mkdir(dir, 0700);
	if (!checkSocketDir(dir)) {
		throw MSXException("Couldn't create socket directory.");
	}
	socketName = StringOp::Builder() << dir << "/socket." << int(getpid());

#ifdef _WIN32
	SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}
	int portNumber = openPort(sd);

	// write port number to file
	FileOperations::unlink(socketName); // ignore error
	std::ofstream out;
	FileOperations::openofstream(out, socketName);
	out << portNumber << std::endl;
	if (!out.good()) {
		sock_close(sd);
		throw MSXException("Couldn't write socket port file.");
	}

#else
	SOCKET sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}

	FileOperations::unlink(socketName); // ignore error

	sockaddr_un addr;
	strcpy(addr.sun_path, socketName.c_str());
	addr.sun_family = AF_UNIX;
	if (bind(sd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
		sock_close(sd);
		throw MSXException("Couldn't bind socket.");
	}
	if (chmod(socketName.c_str(), 0600) == -1) {
		sock_close(sd);
		throw MSXException("Couldn't set socket permissions.");
	}

#endif
	if (!checkSocket(socketName)) {
		sock_close(sd);
		throw MSXException("Opened socket fails sanity check.");
	}
	if (listen(sd, SOMAXCONN) == SOCKET_ERROR) {
		sock_close(sd);
		throw MSXException("Couldn't listen to socket: " + sock_error());
	}
	return sd;
}

void CliServer::exitAcceptLoop()
{
	exitLoop = true;
	sock_close(listenSock);
#ifdef _WIN32
	// Closing the socket is sufficient to make Windows exit the accept() call.
#else
	// The BSD socket API does not contain a simple way to cancel a call to
	// accept(). As a workaround, we look for I/O on an internal pipe.
	char dummy = 'X';
	if (write(wakeupPipe[1], &dummy, sizeof(dummy)) == -1) {
		// Nothing we can do here; we'll have to rely on the poll() timeout.
	}
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
#ifndef _WIN32
	if (pipe(wakeupPipe)) {
		wakeupPipe[0] = wakeupPipe[1] = -1;
		cliComm.printWarning(
				StringOp::Builder() << "Not starting CliServer because "
				"wakeup pipe could not be created: " << strerror(errno));
		return;
	}
#endif

	sock_startup();
	try {
		listenSock = createSocket();
		thread.start();
	} catch (MSXException& e) {
		cliComm.printWarning(e.getMessage());
	}
}

CliServer::~CliServer()
{
	if (listenSock != OPENMSX_INVALID_SOCKET) {
		exitAcceptLoop();
		thread.join();
	}

#ifndef _WIN32
	close(wakeupPipe[0]);
	close(wakeupPipe[1]);
#endif

	deleteSocket(socketName);
	sock_cleanup();
}

void CliServer::run()
{
	mainLoop();
}

void CliServer::mainLoop()
{
#ifndef _WIN32
	// Set socket to non-blocking to make sure accept() doesn't hang when
	// a connection attempt is dropped between poll() and accept().
	fcntl(listenSock, F_SETFL, O_NONBLOCK);
#endif
	while (true) {
		// wait for incoming connection
#ifndef _WIN32
		struct pollfd fds[2] = {
			{ .fd = listenSock, .events = POLLIN },
			{ .fd = wakeupPipe[0], .events = POLLIN },
		};
		int pollResult = poll(fds, 2, 1000);
		if (exitLoop) {
			break;
		}
		if (pollResult == -1) { // error
			break;
		}
		if (pollResult == 0) { // timeout
			continue;
		}
#endif
		SOCKET sd = accept(listenSock, nullptr, nullptr);
		if (exitLoop) {
			break;
		}
		if (sd == OPENMSX_INVALID_SOCKET) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			} else {
				break;
			}
		}
		cliComm.addListener(make_unique<SocketConnection>(
			commandController, eventDistributor, sd));
	}
}

} // namespace openmsx
